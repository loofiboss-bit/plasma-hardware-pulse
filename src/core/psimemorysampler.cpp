#include "psimemorysampler.h"
#include "sysfsreader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    m_diskCounters.clear();
    m_netCounters.clear();
}

void PsiMemorySampler::setIncludedNetworkInterfaces(const QStringList &interfaces)
{
    if (m_includedNetworkInterfaces == interfaces) return;
    m_includedNetworkInterfaces = interfaces;
    m_netCounters.clear();
}

PsiMetric PsiMemorySampler::parsePsiContent(const QString &content, quint64 prevTotalUs, qint64 elapsedMs)
{
    PsiMetric metric;
    bool someTotalValid = false;
    bool fullTotalValid = false;
    bool someAvg60Valid = false;
    bool fullAvg60Valid = false;
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
                    someAvg60Valid = true;
                } else {
                    metric.fullAvg60 = value;
                    fullAvg60Valid = true;
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

    metric.someAvailable = metric.someAvailable && someAvg60Valid && someTotalValid;
    metric.fullAvailable = metric.fullAvailable && fullAvg60Valid && fullTotalValid;
    metric.available = metric.someAvailable || metric.fullAvailable;

    if (metric.someAvailable && prevTotalUs > 0 && metric.someTotalUs >= prevTotalUs && elapsedMs > 50) {
        const double deltaMs = static_cast<double>(metric.someTotalUs - prevTotalUs) / 1000.0;
        const double elapsedSec = static_cast<double>(elapsedMs) / 1000.0;
        metric.deltaStallMsPerSec = std::clamp(deltaMs / elapsedSec, 0.0, 1000.0);
    }

    return metric;
}

void PsiMemorySampler::resetBaselines()
{
    m_prevCpuSomeUs = 0;
    m_prevMemSomeUs = 0;
    m_prevIoSomeUs = 0;
    m_diskCounters.clear();
    m_netCounters.clear();
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
        return QString::fromUtf8(f.readAll());
    };

    const QString cpuPsiText = readWholeText(procDir.filePath(QStringLiteral("pressure/cpu")));
    const QString memPsiText = readWholeText(procDir.filePath(QStringLiteral("pressure/memory")));
    const QString ioPsiText = readWholeText(procDir.filePath(QStringLiteral("pressure/io")));

    if (elapsedMs > 30000) {
        resetBaselines();
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

    // Keep baselines per device: hotplug must never add a device's lifetime counters.
    const double mibPerSecond = elapsedMs > 50 ? 1000.0 / elapsedMs / (1024.0 * 1024.0) : 0.0;
    QHash<QString, Counters> nextDisks;
    static const QRegularExpression wholeDiskRe(QStringLiteral("^(nvme\\d+n\\d+|sd[a-z]+|vd[a-z]+|xvd[a-z]+|mmcblk\\d+)$"));
    const QString diskStatsText = readWholeText(procDir.filePath(QStringLiteral("diskstats")));
    for (const QString &line : diskStatsText.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QStringList cols = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (cols.size() < 11) continue;
        const QDir blockDir(QDir(m_config.sysRoot).filePath(QStringLiteral("class/block/") + cols.at(2)));
        DiskTelemetry item;
        item.name = cols.at(2);
        item.isPartition = QFileInfo::exists(blockDir.filePath(QStringLiteral("partition")));
        const QString canonicalPath = QFileInfo(blockDir.absolutePath()).canonicalFilePath();
        item.isPhysical = !item.isPartition && (QFileInfo::exists(blockDir.filePath(QStringLiteral("device")))
            || (wholeDiskRe.match(item.name).hasMatch() && !canonicalPath.contains(QStringLiteral("/virtual/"))));
        item.included = item.isPhysical;
        const QString diskSequence = SysfsReader::readTrimmedLine(blockDir.filePath(QStringLiteral("diskseq")));
        item.identity = cols.at(0) + QLatin1Char(':') + cols.at(1) + QLatin1Char(':') + item.name + QLatin1Char(':') + diskSequence;
        bool valid = true;
        auto counter = [&cols, &valid](int index) { bool ok = false; const auto n = cols.at(index).toULongLong(&ok); valid = valid && ok; return n; };
        Counters current;
        current.received = counter(5); current.sent = counter(9);
        current.reads = counter(3); current.writes = counter(7);
        const auto readMs = counter(6); const auto writeMs = counter(10);
        current.requestMs = readMs + writeMs;
        if (!valid) continue;
        nextDisks.insert(item.identity, current);
        if (m_diskCounters.contains(item.identity) && mibPerSecond > 0.0) {
            const Counters previous = m_diskCounters.value(item.identity);
            if (current.received >= previous.received && current.sent >= previous.sent
                && current.reads >= previous.reads && current.writes >= previous.writes && current.requestMs >= previous.requestMs) {
                item.throughputAvailable = true;
                item.readMibPerSec = (current.received - previous.received) * 512.0 * mibPerSecond;
                item.writeMibPerSec = (current.sent - previous.sent) * 512.0 * mibPerSecond;
                item.readIops = (current.reads - previous.reads) * 1000.0 / elapsedMs;
                item.writeIops = (current.writes - previous.writes) * 1000.0 / elapsedMs;
                const auto requests = current.reads - previous.reads + current.writes - previous.writes;
                item.requestTimeAvailable = requests > 0;
                if (requests) item.averageRequestMs = double(current.requestMs - previous.requestMs) / requests;
                if (item.included) {
                    snap.diskThroughputAvailable = true;
                    snap.diskReadMibPerSec += item.readMibPerSec;
                    snap.diskWriteMibPerSec += item.writeMibPerSec;
                }
            }
        }
        snap.disks.append(item);
    }
    m_diskCounters = std::move(nextDisks);

    const QDir nvmeClassDir(QDir(m_config.sysRoot).filePath(QStringLiteral("class/nvme")));
    if (nvmeClassDir.exists()) {
        static const QRegularExpression ctrlRe(QStringLiteral("^nvme\\d+$"));
        const QStringList nvmeEntries = nvmeClassDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &entry : nvmeEntries) {
            if (!ctrlRe.match(entry).hasMatch()) continue;
            const QDir ctrlDir(nvmeClassDir.filePath(entry));
            NvmeControllerTelemetry ctrl;
            ctrl.id = entry;
            ctrl.model = SysfsReader::readTrimmedLine(ctrlDir.filePath(QStringLiteral("model")));
            ctrl.state = SysfsReader::readTrimmedLine(ctrlDir.filePath(QStringLiteral("state")));

            const QString critWarnFile = ctrlDir.filePath(QStringLiteral("critical_warning"));
            if (QFileInfo::exists(critWarnFile)) {
                bool ok = false;
                const auto val = SysfsReader::readInt64(critWarnFile, 0, &ok);
                if (ok) {
                    ctrl.warningAvailable = true;
                    ctrl.criticalWarning = static_cast<quint8>(val & 0xFF);
                    ctrl.criticalWarningAlarm = (ctrl.criticalWarning != 0);
                }
            }

            const QStringList hwmonDirs = ctrlDir.entryList(QStringList{QStringLiteral("hwmon*")}, QDir::Dirs, QDir::Name);
            for (const QString &hwDirName : hwmonDirs) {
                const QDir hwDir(ctrlDir.filePath(hwDirName));
                bool tOk = false;
                const double tempC = SysfsReader::readScaledDouble(hwDir.filePath(QStringLiteral("temp1_input")), 1000.0, 0.0, &tOk);
                if (tOk) {
                    ctrl.compositeTempAvailable = true;
                    ctrl.compositeTempCelsius = tempC;
                }
                bool aOk = false;
                const auto alarmVal = SysfsReader::readInt64(hwDir.filePath(QStringLiteral("temp1_alarm")), 0, &aOk);
                if (aOk && alarmVal > 0) {
                    ctrl.criticalWarningAlarm = true;
                }
            }

            snap.nvmeControllers.append(ctrl);

            for (DiskTelemetry &disk : snap.disks) {
                if (disk.name.startsWith(entry) && disk.name.contains(QLatin1Char('n'))) {
                    disk.nvmeWarningAvailable = ctrl.warningAvailable;
                    disk.nvmeCriticalWarning = ctrl.criticalWarning;
                    disk.nvmeState = ctrl.state;
                    disk.nvmeModel = ctrl.model;
                }
            }
        }
    }

    QHash<QString, Counters> nextInterfaces;
    const QString netDevText = readWholeText(procDir.filePath(QStringLiteral("net/dev")));
    for (const QString &line : netDevText.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) continue;
        const QString name = line.left(colon).trimmed();
        const QStringList cols = line.mid(colon + 1).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (cols.size() < 9) continue;
        bool rxOk = false, txOk = false;
        Counters current{cols.at(0).toULongLong(&rxOk), cols.at(8).toULongLong(&txOk)};
        if (!rxOk || !txOk) continue;
        const QDir interfaceDir(QDir(m_config.sysRoot).filePath(QStringLiteral("class/net/") + name));
        NetworkInterfaceTelemetry item;
        item.name = name;
        if (cols.size() >= 12) {
            bool rxErrorOk = false, txErrorOk = false, rxDropOk = false, txDropOk = false;
            item.rxErrors = cols.at(2).toULongLong(&rxErrorOk);
            item.rxDropped = cols.at(3).toULongLong(&rxDropOk);
            item.txErrors = cols.at(10).toULongLong(&txErrorOk);
            item.txDropped = cols.at(11).toULongLong(&txDropOk);
            item.countersAvailable = rxErrorOk && txErrorOk && rxDropOk && txDropOk;
        }
        item.isPhysical = QFileInfo::exists(interfaceDir.filePath(QStringLiteral("device")));
        item.isWireless = QFileInfo::exists(interfaceDir.filePath(QStringLiteral("wireless")))
                       || QFileInfo::exists(interfaceDir.filePath(QStringLiteral("phy80211")));
        item.duplexMode = SysfsReader::readTrimmedLine(interfaceDir.filePath(QStringLiteral("duplex")));
        const QString speedStr = SysfsReader::readTrimmedLine(interfaceDir.filePath(QStringLiteral("speed")));
        bool speedOk = false;
        const int speedVal = speedStr.toInt(&speedOk);
        if (speedOk && speedVal > 0) item.linkSpeedMbps = speedVal;
        const QString carrierStr = SysfsReader::readTrimmedLine(interfaceDir.filePath(QStringLiteral("carrier")));
        item.carrier = (carrierStr == QStringLiteral("1"));
        const QString ifindex = SysfsReader::readTrimmedLine(interfaceDir.filePath(QStringLiteral("ifindex")));
        item.identity = name + QLatin1Char(':') + ifindex;
        item.included = m_includedNetworkInterfaces.isEmpty() ? item.isPhysical : m_includedNetworkInterfaces.contains(name);
        nextInterfaces.insert(item.identity, current);
        if (m_netCounters.contains(item.identity) && mibPerSecond > 0.0) {
            const Counters previous = m_netCounters.value(item.identity);
            if (current.received >= previous.received && current.sent >= previous.sent) {
                item.throughputAvailable = true;
                item.rxMibPerSec = (current.received - previous.received) * mibPerSecond;
                item.txMibPerSec = (current.sent - previous.sent) * mibPerSecond;
                if (item.included) {
                    snap.netThroughputAvailable = true;
                    snap.netRxMibPerSec += item.rxMibPerSec;
                    snap.netTxMibPerSec += item.txMibPerSec;
                }
            }
        }
        snap.networkInterfaces.append(item);
    }
    m_netCounters = std::move(nextInterfaces);

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
