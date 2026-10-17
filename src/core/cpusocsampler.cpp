#include "cpusocsampler.h"
#include "sysfsreader.h"

#include <QDir>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <unistd.h>

namespace LoofiPulse
{

CpuSocSampler::CpuSocSampler(SysfsRootConfig config)
    : m_config(std::move(config))
{
}

void CpuSocSampler::setRootConfig(const SysfsRootConfig &config)
{
    m_config = config;
    m_prevCoreJiffies.clear();
    m_prevTotalJiffies = {};
    m_prevThrottleCounters.clear();
    m_prevProcTicks.clear();
    m_prevSystemTotalTicks = 0;
}

CpuSocSnapshot CpuSocSampler::sampleCpu(const QList<DomainSensorReading> &domainReadings, qint64 elapsedMs)
{
    CpuSocSnapshot snap;

    if (elapsedMs > 30000) {
        m_prevTotalJiffies = {};
        m_prevCoreJiffies.clear();
        m_prevThrottleCounters.clear();
        m_prevProcTicks.clear();
        m_prevSystemTotalTicks = 0;
    }

    // 1. Parse /proc/stat for aggregate and per-core load
    QFile statFile(QDir(m_config.procRoot).filePath(QStringLiteral("stat")));
    QHash<int, double> coreLoads;
    QHash<int, CpuJiffies> currentCoreJiffies;
    QSet<int> validCoreLoads;
    bool aggregateCountersRead = false;

    if (statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = QString::fromUtf8(statFile.read(16384));
        const QStringList lines = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            if (!line.startsWith(QStringLiteral("cpu"))) {
                continue;
            }
            const QStringList cols = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (cols.size() < 8) {
                continue;
            }
            const QString cpuTag = cols.at(0);
            bool ok = false;
            const quint64 user = cols.at(1).toULongLong(&ok);
            if (!ok) continue;
            const quint64 nice = cols.at(2).toULongLong(&ok);
            if (!ok) continue;
            const quint64 system = cols.at(3).toULongLong(&ok);
            if (!ok) continue;
            const quint64 idle = cols.at(4).toULongLong(&ok);
            if (!ok) continue;
            const quint64 iowait = cols.at(5).toULongLong(&ok);
            if (!ok) continue;
            const quint64 irq = cols.at(6).toULongLong(&ok);
            if (!ok) continue;
            const quint64 softirq = cols.at(7).toULongLong(&ok);
            if (!ok) continue;
            const quint64 steal = (cols.size() > 8) ? cols.at(8).toULongLong(&ok) : 0ULL;
            if (!ok) continue;

            const quint64 active = user + nice + system + irq + softirq + steal;
            const quint64 total = active + idle + iowait;

            if (cpuTag == QStringLiteral("cpu")) {
                snap.loadAvailable = true;
                aggregateCountersRead = true;
                if (m_prevTotalJiffies.total > 0 && total > m_prevTotalJiffies.total && active >= m_prevTotalJiffies.active) {
                    snap.loadWarmedUp = true;
                    const double dActive = static_cast<double>(active - m_prevTotalJiffies.active);
                    const double dTotal = static_cast<double>(total - m_prevTotalJiffies.total);
                    snap.totalLoadPercent = std::clamp((dActive * 100.0) / dTotal, 0.0, 100.0);
                }
                m_prevTotalJiffies = {active, total};
            } else {
                bool ok = false;
                const int coreIdx = cpuTag.mid(3).toInt(&ok);
                if (ok) {
                    double coreLoad = 0.0;
                    bool coreLoadAvailable = false;
                    if (m_prevCoreJiffies.contains(coreIdx)) {
                        const CpuJiffies prev = m_prevCoreJiffies.value(coreIdx);
                        if (total > prev.total && active >= prev.active) {
                            const double dActive = static_cast<double>(active - prev.active);
                            const double dTotal = static_cast<double>(total - prev.total);
                            coreLoad = std::clamp((dActive * 100.0) / dTotal, 0.0, 100.0);
                            coreLoadAvailable = true;
                        }
                    }
                    currentCoreJiffies.insert(coreIdx, {active, total});
                    coreLoads.insert(coreIdx, coreLoad);
                    if (coreLoadAvailable) {
                        validCoreLoads.insert(coreIdx);
                    }
                }
            }
        }
    }

    if (aggregateCountersRead) {
        m_prevCoreJiffies = std::move(currentCoreJiffies);
    } else {
        // A missing or malformed aggregate row breaks both aggregate and per-core deltas.
        m_prevTotalJiffies = {};
        m_prevCoreJiffies.clear();
        m_prevProcTicks.clear();
        m_prevSystemTotalTicks = 0;
        coreLoads.clear();
        validCoreLoads.clear();
        snap.loadAvailable = false;
        snap.loadWarmedUp = false;
    }

    // 2. Match package & per-core temperatures from hwmon readings
    QHash<int, double> coreTemps;
    bool packageSensorPreferred = false;
    for (const DomainSensorReading &r : domainReadings) {
        if (r.domain != PhysicalDomain::CpuSoc || !r.available) {
            continue;
        }
        const QString lowerLabel = r.label.toLower();
        const bool preferred = lowerLabel.contains(QStringLiteral("package"))
            || lowerLabel.contains(QStringLiteral("tctl")) || lowerLabel.contains(QStringLiteral("tdie"));
        if (!snap.temperatureAvailable || (preferred && !packageSensorPreferred)
            || (preferred == packageSensorPreferred && r.currentCelsius > snap.packageTempCelsius)) {
            snap.packageTempCelsius = r.currentCelsius;
            snap.packageMaxCelsius = r.maxCelsius;
            snap.thermalHeadroomCelsius = r.headroomCelsius;
            snap.thermalVelocityCPerSec = r.velocityCelsiusPerSec;
            snap.temperatureAvailable = true;
            snap.thermalLimitAvailable = r.limitsAvailable;
            packageSensorPreferred = preferred;
        }
        if (lowerLabel.startsWith(QStringLiteral("core "))) {
            bool ok = false;
            const int cId = lowerLabel.mid(5).trimmed().toInt(&ok);
            if (ok) {
                coreTemps.insert(cId, r.currentCelsius);
            }
        }
    }

    // 3. Read /sys/devices/system/cpu/cpu* frequencies and thermal_throttle counters
    const QDir cpuRoot(QDir(m_config.sysRoot).filePath(QStringLiteral("devices/system/cpu")));
    QList<int> coreIds = coreLoads.keys();
    std::sort(coreIds.begin(), coreIds.end());

    qint64 freqSum = 0;
    qint64 maxFreqSum = 0;
    int freqCount = 0;
    qint64 maxPackageThrottle = 0;
    qint64 totalCoreThrottle = 0;
    QHash<QString, qint64> currentThrottleCounters;

    for (int coreId : coreIds) {
        const QString cpuDir = cpuRoot.filePath(QStringLiteral("cpu%1").arg(coreId));
        bool curOk = false;
        const qint64 curKhz = SysfsReader::readInt64(cpuDir + QStringLiteral("/cpufreq/scaling_cur_freq"), 0, &curOk);
        bool maxOk = false;
        const qint64 maxKhz = SysfsReader::readInt64(cpuDir + QStringLiteral("/cpufreq/scaling_max_freq"), 0, &maxOk);

        const int curMhz = curOk ? static_cast<int>(curKhz / 1000) : 0;
        const int maxMhz = maxOk ? static_cast<int>(maxKhz / 1000) : 0;

        if (curMhz > 0) {
            freqSum += curMhz;
            maxFreqSum += maxMhz;
            ++freqCount;
            snap.peakCoreFreqMhz = std::max(snap.peakCoreFreqMhz, curMhz);
        }

        bool coreThrOk = false;
        bool pkgThrOk = false;
        const qint64 coreThr = SysfsReader::readInt64(cpuDir + QStringLiteral("/thermal_throttle/core_throttle_count"), 0, &coreThrOk);
        const qint64 pkgThr = SysfsReader::readInt64(cpuDir + QStringLiteral("/thermal_throttle/package_throttle_count"), 0, &pkgThrOk);
        coreThrOk = coreThrOk && coreThr >= 0;
        pkgThrOk = pkgThrOk && pkgThr >= 0;
        snap.throttleCountersAvailable = snap.throttleCountersAvailable || coreThrOk || pkgThrOk;
        if (coreThrOk) {
            currentThrottleCounters.insert(QStringLiteral("core/%1").arg(coreId), coreThr);
            totalCoreThrottle += coreThr;
        }
        if (pkgThrOk) {
            currentThrottleCounters.insert(QStringLiteral("package/%1").arg(coreId), pkgThr);
            maxPackageThrottle = std::max(maxPackageThrottle, pkgThr);
        }

        CoreTelemetry ct;
        ct.coreId = coreId;
        ct.loadAvailable = validCoreLoads.contains(coreId);
        ct.loadPercent = coreLoads.value(coreId, 0.0);
        ct.currentFreqMhz = curMhz;
        ct.maxFreqMhz = maxMhz;
        ct.tempCelsius = coreTemps.value(coreId, snap.packageTempCelsius);
        ct.throttleCount = (coreThrOk ? coreThr : 0) + (pkgThrOk ? pkgThr : 0);
        snap.cores.append(ct);
    }

    if (freqCount > 0) {
        snap.frequencyAvailable = true;
        snap.avgFreqMhz = static_cast<int>(freqSum / freqCount);
        snap.maxFreqMhz = static_cast<int>(maxFreqSum / freqCount);
    }

    snap.totalThrottleEvents = std::max(maxPackageThrottle, totalCoreThrottle);
    const bool sameCounterSet = currentThrottleCounters.size() == m_prevThrottleCounters.size()
        && std::all_of(currentThrottleCounters.keyBegin(), currentThrottleCounters.keyEnd(), [this, &currentThrottleCounters](const QString &key) {
            return m_prevThrottleCounters.contains(key)
                && currentThrottleCounters.value(key) >= m_prevThrottleCounters.value(key);
        });
    if (snap.throttleCountersAvailable && sameCounterSet) {
        qint64 coreDelta = 0;
        qint64 packageDelta = 0;
        for (auto it = currentThrottleCounters.cbegin(); it != currentThrottleCounters.cend(); ++it) {
            const qint64 delta = it.value() - m_prevThrottleCounters.value(it.key());
            if (it.key().startsWith(QStringLiteral("core/"))) coreDelta += delta;
            else packageDelta = std::max(packageDelta, delta);
        }
        snap.deltaThrottleEvents = std::max(coreDelta, packageDelta);
    }
    m_prevThrottleCounters = std::move(currentThrottleCounters);

    return snap;
}

GpuSnapshot CpuSocSampler::sampleGpu(const QList<DomainSensorReading> &domainReadings)
{
    GpuSnapshot gpu;
    const QDir drmRoot(QDir(m_config.sysRoot).filePath(QStringLiteral("class/drm")));
    if (!drmRoot.exists()) {
        return gpu;
    }

    const QStringList cards = drmRoot.entryList(QStringList{QStringLiteral("card*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &card : cards) {
        if (card.contains(QLatin1Char('-'))) {
            continue; // skip connector subdirectories like card1-eDP-1
        }
        const QDir cardDir(drmRoot.filePath(card));
        const QString devicePath = QFileInfo(cardDir.filePath(QStringLiteral("device"))).canonicalFilePath();

        bool actOk = false;
        int actMhz = static_cast<int>(SysfsReader::readInt64(cardDir.filePath(QStringLiteral("gt_act_freq_mhz")), 0, &actOk));
        if (!actOk) {
            actMhz = static_cast<int>(SysfsReader::readInt64(cardDir.filePath(QStringLiteral("gt/gt0/rps_act_freq_mhz")), 0, &actOk));
        }
        if (!actOk) {
            actMhz = static_cast<int>(SysfsReader::readInt64(cardDir.filePath(QStringLiteral("device/tile0/gt0/freq0/act_freq")), 0, &actOk));
        }
        int boostMhz = static_cast<int>(SysfsReader::readInt64(cardDir.filePath(QStringLiteral("gt_boost_freq_mhz")), 0));
        if (boostMhz <= 0) {
            boostMhz = static_cast<int>(SysfsReader::readInt64(cardDir.filePath(QStringLiteral("gt_max_freq_mhz")), 0));
        }
        if (boostMhz <= 0) {
            boostMhz = static_cast<int>(SysfsReader::readInt64(cardDir.filePath(QStringLiteral("device/tile0/gt0/freq0/max_freq")), 0));
        }

        bool busyOk = false;
        const qint64 amdBusy = SysfsReader::readInt64(cardDir.filePath(QStringLiteral("device/gpu_busy_percent")), 0, &busyOk);

        if (actOk || busyOk) {
            gpu.available = true;
            gpu.sysfsPath = devicePath;
            gpu.currentFreqMhz = actMhz;
            gpu.frequencyAvailable = actOk;
            gpu.boostFreqMhz = boostMhz;
            if (busyOk) {
                gpu.busyPercent = static_cast<double>(amdBusy);
                gpu.busyAvailable = amdBusy >= 0 && amdBusy <= 100;
            }
            gpu.driverName = QFileInfo(cardDir.filePath(QStringLiteral("device/driver"))).symLinkTarget().section(QLatin1Char('/'), -1);
            if (gpu.driverName.isEmpty()) gpu.driverName = QStringLiteral("DRM");

            bool vramUsedOk = false;
            const qint64 vramUsedBytes = SysfsReader::readInt64(cardDir.filePath(QStringLiteral("device/mem_info_vram_used")), 0, &vramUsedOk);
            const qint64 vramTotalBytes = SysfsReader::readInt64(cardDir.filePath(QStringLiteral("device/mem_info_vram_total")), 0);
            if (vramUsedOk && vramTotalBytes > 0) {
                gpu.vramAvailable = true;
                gpu.vramUsedMegabytes = static_cast<double>(vramUsedBytes) / (1024.0 * 1024.0);
                gpu.vramTotalMegabytes = static_cast<double>(vramTotalBytes) / (1024.0 * 1024.0);
            }

            const QDir gpuHwmonDir(cardDir.filePath(QStringLiteral("device/hwmon")));
            if (gpuHwmonDir.exists()) {
                const QStringList subHwmons = gpuHwmonDir.entryList(QStringList{QStringLiteral("hwmon*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                for (const QString &sub : subHwmons) {
                    const QDir subDir(gpuHwmonDir.filePath(sub));
                    bool pOk = false;
                    double pWatts = SysfsReader::readScaledDouble(subDir.filePath(QStringLiteral("power1_average")), 1000000.0, 0.0, &pOk);
                    if (!pOk || pWatts < 0.0) {
                        pWatts = SysfsReader::readScaledDouble(subDir.filePath(QStringLiteral("power1_input")), 1000000.0, 0.0, &pOk);
                    }
                    if (pOk && pWatts >= 0.0) {
                        gpu.powerWatts = pWatts;
                        gpu.powerAvailable = true;
                        break;
                    }
                }
            }
        }

        const QDir hwmonDir(cardDir.filePath(QStringLiteral("device/hwmon")));
        QSet<QString> deviceHwmonPaths;
        const QStringList hwmonDirs = hwmonDir.entryList(QStringList{QStringLiteral("hwmon*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &hwmon : hwmonDirs) {
            deviceHwmonPaths.insert(QFileInfo(hwmonDir.filePath(hwmon)).canonicalFilePath());
        }
        for (const DomainSensorReading &reading : domainReadings) {
            if (reading.domain == PhysicalDomain::Graphics && !reading.sysfsPath.isEmpty()
                && deviceHwmonPaths.contains(reading.sysfsPath)) {
                gpu.tempCelsius = reading.currentCelsius;
                gpu.temperatureAvailable = reading.available;
                gpu.available = true;
                gpu.sysfsPath = devicePath;
                if (gpu.driverName.isEmpty()) gpu.driverName = reading.chipName;
            }
        }
        if (gpu.available && gpu.sysfsPath == devicePath) {
            break;
        }
    }

    return gpu;
}

CulpritProcess CpuSocSampler::sampleTopCulpritProcess()
{
    return sampleProcessRadar(1).topCpuProcess;
}

ProcessRadarSnapshot CpuSocSampler::sampleProcessRadar(int maxTopProcesses)
{
    ProcessRadarSnapshot radar;
    radar.observedAtMs = QDateTime::currentMSecsSinceEpoch();
    const QDir procDir(m_config.procRoot);
    if (!procDir.exists()) {
        return radar;
    }

    const quint64 currentSysTicks = m_prevTotalJiffies.total;
    const quint64 deltaSysTicks = (m_prevSystemTotalTicks > 0 && currentSysTicks > m_prevSystemTotalTicks)
        ? (currentSysTicks - m_prevSystemTotalTicks)
        : 100ULL;
    m_prevSystemTotalTicks = currentSysTicks;

    QDirIterator procIt(m_config.procRoot, QDir::Dirs | QDir::NoDotAndDotDot);
    QHash<int, ProcSample> nextProcTicks;
    nextProcTicks.reserve(m_prevProcTicks.size());
    QList<CulpritProcess> candidates;

    const long pageSizeKb = std::max(4L, sysconf(_SC_PAGESIZE) / 1024L);
    const int numCpus = std::max(1, static_cast<int>(m_prevCoreJiffies.size()));

    while (procIt.hasNext()) {
        procIt.next();
        const QString entry = procIt.fileName();
        bool pidOk = false;
        const int pid = entry.toInt(&pidOk);
        if (!pidOk || pid <= 1) {
            continue;
        }

        const QString statLine = SysfsReader::readTrimmedLine(procIt.filePath() + QStringLiteral("/stat"));
        if (statLine.isEmpty()) {
            continue;
        }

        const int openParen = statLine.indexOf(QLatin1Char('('));
        const int closeParen = statLine.lastIndexOf(QLatin1Char(')'));
        if (openParen < 0 || closeParen <= openParen || closeParen + 2 >= statLine.size()) {
            continue;
        }

        const QString comm = statLine.mid(openParen + 1, closeParen - openParen - 1);
        const QStringList rest = statLine.mid(closeParen + 2).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        // After ')': index 0 is state, 11 is utime, 12 is stime, 21 is rss (in pages)
        if (rest.size() <= 21) {
            continue;
        }

        const QString procState = rest.at(0);
        const bool isDiskWait = (procState == QStringLiteral("D"));
        bool userOk = false, systemOk = false, startOk = false;
        const quint64 utime = rest.at(11).toULongLong(&userOk);
        const quint64 stime = rest.at(12).toULongLong(&systemOk);
        const quint64 startTime = rest.at(19).toULongLong(&startOk);
        if (!userOk || !systemOk || !startOk) continue;
        const quint64 totalTicks = utime + stime;
        nextProcTicks.insert(pid, {totalTicks, startTime, comm, 0.0});

        const qint64 rssPages = std::max<qint64>(0, rest.at(21).toLongLong());
        const double rssMb = static_cast<double>(rssPages * pageSizeKb) / 1024.0;

        double cpuPct = 0.0;
        if (m_prevProcTicks.contains(pid) && m_prevProcTicks.value(pid).startTimeTicks == startTime) {
            const quint64 prevTicks = m_prevProcTicks.value(pid).ticks;
            if (totalTicks > prevTicks) {
                const quint64 dTicks = totalTicks - prevTicks;
                cpuPct = std::clamp(
                    (static_cast<double>(dTicks) * 100.0 * static_cast<double>(numCpus)) / static_cast<double>(deltaSysTicks),
                    0.0,
                    100.0 * static_cast<double>(numCpus));
            }
        }

        if (isDiskWait) {
            ++radar.diskWaitProcessCount;
            if (radar.topDiskWaitProcess.pid == 0
                || cpuPct > radar.topDiskWaitProcess.cpuPercent
                || rssMb > radar.topDiskWaitProcess.rssMegabytes) {
                radar.topDiskWaitProcess.startTimeTicks = startTime;
                radar.topDiskWaitProcess.observedAtMs = radar.observedAtMs;
                radar.topDiskWaitProcess.pid = pid;
                radar.topDiskWaitProcess.name = comm;
                radar.topDiskWaitProcess.cpuPercent = cpuPct;
                radar.topDiskWaitProcess.rssMegabytes = rssMb;
                radar.topDiskWaitProcess.state = procState;
                radar.topDiskWaitProcess.inDiskWait = true;
            }
        }

        if (cpuPct > 0.05 || rssMb >= 8.0 || isDiskWait) {
            CulpritProcess cp;
            cp.startTimeTicks = startTime;
            cp.observedAtMs = radar.observedAtMs;
            cp.pid = pid;
            cp.name = comm;
            cp.cpuPercent = cpuPct;
            cp.rssMegabytes = rssMb;
            cp.state = procState;
            cp.inDiskWait = isDiskWait;
            candidates.append(cp);
        }

        if (cpuPct > radar.topCpuProcess.cpuPercent) {
            radar.topCpuProcess.startTimeTicks = startTime;
            radar.topCpuProcess.observedAtMs = radar.observedAtMs;
            radar.topCpuProcess.pid = pid;
            radar.topCpuProcess.name = comm;
            radar.topCpuProcess.cpuPercent = cpuPct;
            radar.topCpuProcess.rssMegabytes = rssMb;
            radar.topCpuProcess.state = procState;
            radar.topCpuProcess.inDiskWait = isDiskWait;
        }

        if (rssMb > radar.topMemoryProcess.rssMegabytes) {
            radar.topMemoryProcess.startTimeTicks = startTime;
            radar.topMemoryProcess.observedAtMs = radar.observedAtMs;
            radar.topMemoryProcess.pid = pid;
            radar.topMemoryProcess.name = comm;
            radar.topMemoryProcess.cpuPercent = cpuPct;
            radar.topMemoryProcess.rssMegabytes = rssMb;
            radar.topMemoryProcess.state = procState;
            radar.topMemoryProcess.inDiskWait = isDiskWait;
        }
    }

    m_prevProcTicks = std::move(nextProcTicks);

    // Sort candidates by combined impact (CPU % weighted + RSS in GiB + Disk Wait priority)
    std::sort(candidates.begin(), candidates.end(), [](const CulpritProcess &a, const CulpritProcess &b) {
        const double scoreA = a.cpuPercent * 2.0 + (a.rssMegabytes / 256.0) + (a.inDiskWait ? 40.0 : 0.0);
        const double scoreB = b.cpuPercent * 2.0 + (b.rssMegabytes / 256.0) + (b.inDiskWait ? 40.0 : 0.0);
        return scoreA > scoreB;
    });

    const int limit = std::min<int>(std::max(1, maxTopProcesses), static_cast<int>(candidates.size()));
    for (int i = 0; i < limit; ++i) {
        radar.topProcesses.append(candidates.at(i));
    }

    return radar;
}

} // namespace LoofiPulse
