#include "psimemorysampler.h"
#include "sysfsreader.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

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
    m_prevDiskReadSectors = 0;
    m_prevDiskWriteSectors = 0;
    m_hasPrevDiskStats = false;
    m_prevNetRxBytes = 0;
    m_prevNetTxBytes = 0;
    m_hasPrevNetStats = false;
}

PsiMetric PsiMemorySampler::parsePsiContent(const QString &content, quint64 prevTotalUs, qint64 elapsedMs)
{
    PsiMetric metric;
    bool someTotalValid = false;
    bool fullTotalValid = false;
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

            bool ok = false;
            if (key == QStringLiteral("avg10")) {
                const double value = valStr.toDouble(&ok);
                if (!ok || !std::isfinite(value) || value < 0.0 || value > 100.0) {
                    continue;
                }
                if (isSome) {
                    metric.someAvg10 = value;
                    metric.someAvailable = true;
                } else {
                    metric.fullAvg10 = value;
                    metric.fullAvailable = true;
                }
            } else if (key == QStringLiteral("avg60")) {
                const double value = valStr.toDouble(&ok);
                if (!ok || !std::isfinite(value) || value < 0.0 || value > 100.0) {
                    continue;
                }
                if (isSome) {
                    metric.someAvg60 = value;
                } else {
                    metric.fullAvg60 = value;
                }
            } else if (key == QStringLiteral("total")) {
                const quint64 value = valStr.toULongLong(&ok);
                if (!ok) {
                    continue;
                }
                if (isSome) {
                    metric.someTotalUs = value;
                    someTotalValid = true;
                } else {
                    metric.fullTotalUs = value;
                    fullTotalValid = true;
                }
            }
        }
    }

    metric.someAvailable = metric.someAvailable && someTotalValid;
    metric.fullAvailable = metric.fullAvailable && fullTotalValid;
    metric.available = metric.someAvailable || metric.fullAvailable;

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

    if (elapsedMs > 30000) {
        m_prevCpuSomeUs = 0;
        m_prevMemSomeUs = 0;
        m_prevIoSomeUs = 0;
        m_prevDiskReadSectors = 0;
        m_prevDiskWriteSectors = 0;
        m_hasPrevDiskStats = false;
        m_prevNetRxBytes = 0;
        m_prevNetTxBytes = 0;
        m_hasPrevNetStats = false;
    }

    snap.cpuPsi = parsePsiContent(cpuPsiText, m_prevCpuSomeUs, elapsedMs);
    snap.memoryPsi = parsePsiContent(memPsiText, m_prevMemSomeUs, elapsedMs);
    snap.ioPsi = parsePsiContent(ioPsiText, m_prevIoSomeUs, elapsedMs);

    const auto updateCounter = [](const PsiMetric &metric, quint64 &previous) {
        if (!metric.someAvailable || metric.someTotalUs < previous) {
            previous = 0;
            return;
        }
        previous = metric.someTotalUs;
    };
    updateCounter(snap.cpuPsi, m_prevCpuSomeUs);
    updateCounter(snap.memoryPsi, m_prevMemSomeUs);
    updateCounter(snap.ioPsi, m_prevIoSomeUs);

    // Parse /proc/diskstats for whole physical block devices (nvme*n*, sd[a-z]+, vd[a-z]+, mmcblk[0-9]+)
    const QString diskStatsText = readWholeText(procDir.filePath(QStringLiteral("diskstats")));
    if (!diskStatsText.isEmpty()) {
        static const QRegularExpression wholeDiskRe(QStringLiteral("^(nvme\\d+n\\d+|sd[a-z]+|vd[a-z]+|mmcblk\\d+)$"));
        quint64 totalReadSectors = 0;
        quint64 totalWriteSectors = 0;
        bool foundDisk = false;
        const QStringList diskLines = diskStatsText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &dLine : diskLines) {
            const QStringList cols = dLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (cols.size() < 10) {
                continue;
            }
            const QString devName = cols.at(2);
            if (!wholeDiskRe.match(devName).hasMatch()) {
                continue;
            }
            bool rOk = false;
            bool wOk = false;
            const quint64 rSec = cols.at(5).toULongLong(&rOk);
            const quint64 wSec = cols.at(9).toULongLong(&wOk);
            if (rOk && wOk) {
                totalReadSectors += rSec;
                totalWriteSectors += wSec;
                foundDisk = true;
            }
        }
        if (foundDisk) {
            if (m_hasPrevDiskStats && elapsedMs > 50
                && totalReadSectors >= m_prevDiskReadSectors
                && totalWriteSectors >= m_prevDiskWriteSectors) {
                const double dtSec = static_cast<double>(elapsedMs) / 1000.0;
                const double readBytes = static_cast<double>(totalReadSectors - m_prevDiskReadSectors) * 512.0;
                const double writeBytes = static_cast<double>(totalWriteSectors - m_prevDiskWriteSectors) * 512.0;
                snap.diskReadMibPerSec = std::clamp((readBytes / (1024.0 * 1024.0)) / dtSec, 0.0, 20000.0);
                snap.diskWriteMibPerSec = std::clamp((writeBytes / (1024.0 * 1024.0)) / dtSec, 0.0, 20000.0);
                snap.diskThroughputAvailable = true;
            }
            m_prevDiskReadSectors = totalReadSectors;
            m_prevDiskWriteSectors = totalWriteSectors;
            m_hasPrevDiskStats = true;
        } else {
            m_hasPrevDiskStats = false;
        }
    } else {
        m_hasPrevDiskStats = false;
    }

    // Parse /proc/net/dev for physical/active network interfaces
    const QString netDevText = readWholeText(procDir.filePath(QStringLiteral("net/dev")));
    if (!netDevText.isEmpty()) {
        quint64 totalRxBytes = 0;
        quint64 totalTxBytes = 0;
        bool foundNet = false;
        const QStringList netLines = netDevText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &nLine : netLines) {
            const int colonIdx = nLine.indexOf(QLatin1Char(':'));
            if (colonIdx <= 0) {
                continue;
            }
            const QString iface = nLine.left(colonIdx).trimmed();
            if (iface == QStringLiteral("lo")
                || iface.startsWith(QStringLiteral("veth"))
                || iface.startsWith(QStringLiteral("docker"))
                || iface.startsWith(QStringLiteral("br-"))
                || iface.startsWith(QStringLiteral("virbr"))) {
                continue;
            }
            const QStringList cols = nLine.mid(colonIdx + 1).split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (cols.size() < 9) {
                continue;
            }
            bool rxOk = false;
            bool txOk = false;
            const quint64 rxBytes = cols.at(0).toULongLong(&rxOk);
            const quint64 txBytes = cols.at(8).toULongLong(&txOk);
            if (rxOk && txOk) {
                totalRxBytes += rxBytes;
                totalTxBytes += txBytes;
                foundNet = true;
            }
        }
        if (foundNet) {
            if (m_hasPrevNetStats && elapsedMs > 50
                && totalRxBytes >= m_prevNetRxBytes
                && totalTxBytes >= m_prevNetTxBytes) {
                const double dtSec = static_cast<double>(elapsedMs) / 1000.0;
                const double dRx = static_cast<double>(totalRxBytes - m_prevNetRxBytes);
                const double dTx = static_cast<double>(totalTxBytes - m_prevNetTxBytes);
                snap.netRxMibPerSec = std::clamp((dRx / (1024.0 * 1024.0)) / dtSec, 0.0, 20000.0);
                snap.netTxMibPerSec = std::clamp((dTx / (1024.0 * 1024.0)) / dtSec, 0.0, 20000.0);
                snap.netThroughputAvailable = true;
            }
            m_prevNetRxBytes = totalRxBytes;
            m_prevNetTxBytes = totalTxBytes;
            m_hasPrevNetStats = true;
        } else {
            m_hasPrevNetStats = false;
        }
    } else {
        m_hasPrevNetStats = false;
    }

    // Parse /proc/meminfo
    const QString memInfoText = readWholeText(procDir.filePath(QStringLiteral("meminfo")));
    const QStringList memLines = memInfoText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    bool totalOk = false;
    bool availableOk = false;
    bool swapTotalOk = false;
    bool swapFreeOk = false;
    for (const QString &line : memLines) {
        if (line.startsWith(QStringLiteral("MemTotal:"))) {
            const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (tokens.size() >= 2) {
                const quint64 value = tokens.at(1).toULongLong(&totalOk);
                if (totalOk) snap.ramTotalBytes = value * 1024ULL;
            }
        } else if (line.startsWith(QStringLiteral("MemAvailable:"))) {
            const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (tokens.size() >= 2) {
                const quint64 value = tokens.at(1).toULongLong(&availableOk);
                if (availableOk) snap.ramAvailableBytes = value * 1024ULL;
            }
        } else if (line.startsWith(QStringLiteral("SwapTotal:"))) {
            const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (tokens.size() >= 2) {
                const quint64 value = tokens.at(1).toULongLong(&swapTotalOk);
                if (swapTotalOk) snap.swapTotalBytes = value * 1024ULL;
            }
        } else if (line.startsWith(QStringLiteral("SwapFree:"))) {
            const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (tokens.size() >= 2) {
                const quint64 value = tokens.at(1).toULongLong(&swapFreeOk);
                if (swapFreeOk) snap.swapFreeBytes = value * 1024ULL;
            }
        }
    }

    snap.swapAvailable = swapTotalOk && swapFreeOk && snap.swapFreeBytes <= snap.swapTotalBytes;
    snap.ramAvailable = totalOk && availableOk && snap.ramTotalBytes > 0 && snap.ramAvailableBytes <= snap.ramTotalBytes;
    if (snap.ramAvailable) {
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
        bool origOk = false;
        bool compressedOk = false;
        bool usedOk = false;
        const quint64 orig = cols.value(0).toULongLong(&origOk);
        const quint64 compressed = cols.value(1).toULongLong(&compressedOk);
        const quint64 used = cols.value(2).toULongLong(&usedOk);
        if (cols.size() >= 3 && origOk && compressedOk && usedOk) {
            snap.zramAvailable = true;
            snap.zramActive = true;
            snap.zramOrigBytes += orig;
            snap.zramCompressedBytes += compressed;
            snap.zramMemUsedTotalBytes += used;
        }
    }

    if (snap.zramCompressedBytes > 0 && snap.zramOrigBytes > 0) {
        snap.zramCompressionAvailable = true;
        snap.zramCompressionRatio = static_cast<double>(snap.zramOrigBytes) / static_cast<double>(snap.zramCompressedBytes);
    }

    // Calculate System Responsiveness Index (0 - 100%)
    // Memory full stall and IO full stall penalize responsiveness more heavily than CPU some stall
    const double penalty = (snap.cpuPsi.someAvg10 * 0.6)
        + (snap.memoryPsi.someAvg10 * 1.4)
        + (snap.memoryPsi.fullAvg10 * 2.5)
        + (snap.ioPsi.someAvg10 * 0.8)
        + (snap.ioPsi.fullAvg10 * 1.6);
    // The index uses memory/io full-stall values as well as all three some-stall values.
    // Do not silently substitute zero for a missing or malformed input.
    snap.responsivenessAvailable = snap.cpuPsi.someAvailable
        && snap.memoryPsi.someAvailable && snap.memoryPsi.fullAvailable
        && snap.ioPsi.someAvailable && snap.ioPsi.fullAvailable;
    snap.sampledAtLeastOnce = true;
    if (snap.responsivenessAvailable) {
        snap.responsivenessScore = std::clamp(100.0 - penalty, 0.0, 100.0);
    }

    return snap;
}

} // namespace LoofiPulse
