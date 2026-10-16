#include "psimemorysampler.h"
#include "sysfsreader.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <algorithm>

namespace LoofiPulse
{

PsiMemorySampler::PsiMemorySampler(SysfsRootConfig config)
    : m_config(std::move(config))
{
}

void PsiMemorySampler::setRootConfig(const SysfsRootConfig &config)
{
    m_config = config;
    m_prevCpuSomeUs = 0;
    m_prevMemSomeUs = 0;
    m_prevIoSomeUs = 0;
    m_hasPrevTotals = false;
}

PsiMetric PsiMemorySampler::parsePsiContent(const QString &content, quint64 prevTotalUs, qint64 elapsedMs)
{
    PsiMetric metric;
    const QStringList lines = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        const bool isSome = line.startsWith(QStringLiteral("some "));
        const bool isFull = line.startsWith(QStringLiteral("full "));
        if (!isSome && !isFull) {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (int i = 1; i < parts.size(); ++i) {
            const QString &token = parts.at(i);
            const int eqIdx = token.indexOf(QLatin1Char('='));
            if (eqIdx <= 0) {
                continue;
            }
            const QString key = token.left(eqIdx);
            const QString valStr = token.mid(eqIdx + 1);

            if (key == QStringLiteral("avg10")) {
                if (isSome) {
                    metric.someAvg10 = valStr.toDouble();
                } else {
                    metric.fullAvg10 = valStr.toDouble();
                }
            } else if (key == QStringLiteral("avg60")) {
                if (isSome) {
                    metric.someAvg60 = valStr.toDouble();
                } else {
                    metric.fullAvg60 = valStr.toDouble();
                }
            } else if (key == QStringLiteral("total")) {
                if (isSome) {
                    metric.someTotalUs = valStr.toULongLong();
                } else {
                    metric.fullTotalUs = valStr.toULongLong();
                }
            }
        }
    }

    if (prevTotalUs > 0 && metric.someTotalUs >= prevTotalUs && elapsedMs > 50) {
        const double deltaMs = static_cast<double>(metric.someTotalUs - prevTotalUs) / 1000.0;
        const double elapsedSec = static_cast<double>(elapsedMs) / 1000.0;
        metric.deltaStallMsPerSec = std::clamp(deltaMs / elapsedSec, 0.0, 1000.0);
    }

    return metric;
}

PsiAndMemorySnapshot PsiMemorySampler::sample(qint64 elapsedMs)
{
    PsiAndMemorySnapshot snap;
    const QDir procDir(m_config.procRoot);

    auto readWholeText = [](const QString &path) -> QString {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        return QString::fromUtf8(f.read(4096));
    };

    const QString cpuPsiText = readWholeText(procDir.filePath(QStringLiteral("pressure/cpu")));
    const QString memPsiText = readWholeText(procDir.filePath(QStringLiteral("pressure/memory")));
    const QString ioPsiText = readWholeText(procDir.filePath(QStringLiteral("pressure/io")));

    snap.cpuPsi = parsePsiContent(cpuPsiText, m_hasPrevTotals ? m_prevCpuSomeUs : 0, elapsedMs);
    snap.memoryPsi = parsePsiContent(memPsiText, m_hasPrevTotals ? m_prevMemSomeUs : 0, elapsedMs);
    snap.ioPsi = parsePsiContent(ioPsiText, m_hasPrevTotals ? m_prevIoSomeUs : 0, elapsedMs);

    m_prevCpuSomeUs = snap.cpuPsi.someTotalUs;
    m_prevMemSomeUs = snap.memoryPsi.someTotalUs;
    m_prevIoSomeUs = snap.ioPsi.someTotalUs;
    m_hasPrevTotals = true;

    // Parse /proc/meminfo
    const QString memInfoText = readWholeText(procDir.filePath(QStringLiteral("meminfo")));
    const QStringList memLines = memInfoText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : memLines) {
        if (line.startsWith(QStringLiteral("MemTotal:"))) {
            const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (tokens.size() >= 2) {
                snap.ramTotalBytes = tokens.at(1).toULongLong() * 1024ULL;
            }
        } else if (line.startsWith(QStringLiteral("MemAvailable:"))) {
            const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (tokens.size() >= 2) {
                snap.ramAvailableBytes = tokens.at(1).toULongLong() * 1024ULL;
            }
        } else if (line.startsWith(QStringLiteral("SwapTotal:"))) {
            const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (tokens.size() >= 2) {
                snap.swapTotalBytes = tokens.at(1).toULongLong() * 1024ULL;
            }
        } else if (line.startsWith(QStringLiteral("SwapFree:"))) {
            const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (tokens.size() >= 2) {
                snap.swapFreeBytes = tokens.at(1).toULongLong() * 1024ULL;
            }
        }
    }

    if (snap.ramTotalBytes > 0) {
        snap.ramUsedBytes = (snap.ramTotalBytes > snap.ramAvailableBytes)
            ? (snap.ramTotalBytes - snap.ramAvailableBytes)
            : 0ULL;
        snap.ramUsedPercent = (static_cast<double>(snap.ramUsedBytes) * 100.0) / static_cast<double>(snap.ramTotalBytes);
    }

    // Parse /sys/block/zram*/mm_stat
    const QDir blockDir(QDir(m_config.sysRoot).filePath(QStringLiteral("block")));
    const QStringList zramDirs = blockDir.entryList(QStringList{QStringLiteral("zram*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &zramName : zramDirs) {
        const QString mmStatLine = SysfsReader::readTrimmedLine(blockDir.filePath(zramName + QStringLiteral("/mm_stat")));
        if (mmStatLine.isEmpty()) {
            continue;
        }
        const QStringList cols = mmStatLine.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (cols.size() >= 3) {
            snap.zramActive = true;
            snap.zramOrigBytes += cols.at(0).toULongLong();
            snap.zramCompressedBytes += cols.at(1).toULongLong();
            snap.zramMemUsedTotalBytes += cols.at(2).toULongLong();
        }
    }

    if (snap.zramCompressedBytes > 0 && snap.zramOrigBytes > 0) {
        snap.zramCompressionRatio = static_cast<double>(snap.zramOrigBytes) / static_cast<double>(snap.zramCompressedBytes);
    }

    // Calculate System Responsiveness Index (0 - 100%)
    // Memory full stall and IO full stall penalize responsiveness more heavily than CPU some stall
    const double penalty = (snap.cpuPsi.someAvg10 * 0.6)
        + (snap.memoryPsi.someAvg10 * 1.4)
        + (snap.memoryPsi.fullAvg10 * 2.5)
        + (snap.ioPsi.someAvg10 * 0.8)
        + (snap.ioPsi.fullAvg10 * 1.6);
    snap.responsivenessScore = std::clamp(100.0 - penalty, 0.0, 100.0);

    return snap;
}

} // namespace LoofiPulse
