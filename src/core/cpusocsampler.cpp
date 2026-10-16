#include "cpusocsampler.h"
#include "sysfsreader.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

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
    m_prevThrottleEvents = -1;
    m_prevProcTicks.clear();
    m_prevSystemTotalTicks = 0;
}

CpuSocSnapshot CpuSocSampler::sampleCpu(const QList<DomainSensorReading> &domainReadings)
{
    CpuSocSnapshot snap;

    // 1. Parse /proc/stat for aggregate and per-core load
    QFile statFile(QDir(m_config.procRoot).filePath(QStringLiteral("stat")));
    QHash<int, double> coreLoads;

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
            const quint64 user = cols.at(1).toULongLong();
            const quint64 nice = cols.at(2).toULongLong();
            const quint64 system = cols.at(3).toULongLong();
            const quint64 idle = cols.at(4).toULongLong();
            const quint64 iowait = cols.at(5).toULongLong();
            const quint64 irq = cols.at(6).toULongLong();
            const quint64 softirq = cols.at(7).toULongLong();
            const quint64 steal = (cols.size() > 8) ? cols.at(8).toULongLong() : 0ULL;

            const quint64 active = user + nice + system + irq + softirq + steal;
            const quint64 total = active + idle + iowait;

            if (cpuTag == QStringLiteral("cpu")) {
                if (m_prevTotalJiffies.total > 0 && total > m_prevTotalJiffies.total) {
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
                    if (m_prevCoreJiffies.contains(coreIdx)) {
                        const CpuJiffies prev = m_prevCoreJiffies.value(coreIdx);
                        if (total > prev.total) {
                            const double dActive = static_cast<double>(active - prev.active);
                            const double dTotal = static_cast<double>(total - prev.total);
                            coreLoad = std::clamp((dActive * 100.0) / dTotal, 0.0, 100.0);
                        }
                    }
                    m_prevCoreJiffies.insert(coreIdx, {active, total});
                    coreLoads.insert(coreIdx, coreLoad);
                }
            }
        }
    }

    // 2. Match package & per-core temperatures from hwmon readings
    QHash<int, double> coreTemps;
    for (const DomainSensorReading &r : domainReadings) {
        if (r.domain != PhysicalDomain::CpuSoc) {
            continue;
        }
        const QString lowerLabel = r.label.toLower();
        if (lowerLabel.contains(QStringLiteral("package")) ||
            lowerLabel.contains(QStringLiteral("tctl")) ||
            lowerLabel.contains(QStringLiteral("tdie")) ||
            snap.packageTempCelsius <= 0.0) {
            if (r.currentCelsius >= snap.packageTempCelsius) {
                snap.packageTempCelsius = r.currentCelsius;
                snap.packageMaxCelsius = r.maxCelsius;
                snap.thermalHeadroomCelsius = r.headroomCelsius;
                snap.thermalVelocityCPerSec = r.velocityCelsiusPerSec;
            }
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

        const qint64 coreThr = SysfsReader::readInt64(cpuDir + QStringLiteral("/thermal_throttle/core_throttle_count"), 0);
        const qint64 pkgThr = SysfsReader::readInt64(cpuDir + QStringLiteral("/thermal_throttle/package_throttle_count"), 0);
        totalCoreThrottle += coreThr;
        maxPackageThrottle = std::max(maxPackageThrottle, pkgThr);

        CoreTelemetry ct;
        ct.coreId = coreId;
        ct.loadPercent = coreLoads.value(coreId, 0.0);
        ct.currentFreqMhz = curMhz;
        ct.maxFreqMhz = maxMhz;
        ct.tempCelsius = coreTemps.value(coreId, snap.packageTempCelsius);
        ct.throttleCount = coreThr + pkgThr;
        snap.cores.append(ct);
    }

    if (freqCount > 0) {
        snap.avgFreqMhz = static_cast<int>(freqSum / freqCount);
        snap.maxFreqMhz = static_cast<int>(maxFreqSum / freqCount);
    }

    snap.totalThrottleEvents = std::max(maxPackageThrottle, totalCoreThrottle);
    if (m_prevThrottleEvents >= 0 && snap.totalThrottleEvents >= m_prevThrottleEvents) {
        snap.deltaThrottleEvents = snap.totalThrottleEvents - m_prevThrottleEvents;
    }
    m_prevThrottleEvents = snap.totalThrottleEvents;

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

        bool actOk = false;
        int actMhz = static_cast<int>(SysfsReader::readInt64(cardDir.filePath(QStringLiteral("gt_act_freq_mhz")), 0, &actOk));
        if (!actOk) {
            actMhz = static_cast<int>(SysfsReader::readInt64(cardDir.filePath(QStringLiteral("gt/gt0/rps_act_freq_mhz")), 0, &actOk));
        }
        int boostMhz = static_cast<int>(SysfsReader::readInt64(cardDir.filePath(QStringLiteral("gt_boost_freq_mhz")), 0));
        if (boostMhz <= 0) {
            boostMhz = static_cast<int>(SysfsReader::readInt64(cardDir.filePath(QStringLiteral("gt_max_freq_mhz")), 0));
        }

        bool busyOk = false;
        const qint64 amdBusy = SysfsReader::readInt64(cardDir.filePath(QStringLiteral("device/gpu_busy_percent")), 0, &busyOk);

        if (actOk || busyOk) {
            gpu.available = true;
            gpu.currentFreqMhz = actMhz;
            gpu.boostFreqMhz = boostMhz;
            if (busyOk) {
                gpu.busyPercent = static_cast<double>(amdBusy);
                gpu.driverName = QStringLiteral("amdgpu");
            } else {
                gpu.driverName = QStringLiteral("Intel Graphics");
                if (boostMhz > 0) {
                    gpu.busyPercent = std::clamp((static_cast<double>(actMhz) * 100.0) / static_cast<double>(boostMhz), 0.0, 100.0);
                }
            }
            break;
        }
    }

    for (const DomainSensorReading &r : domainReadings) {
        if (r.domain == PhysicalDomain::Graphics) {
            gpu.tempCelsius = r.currentCelsius;
            gpu.available = true;
            break;
        }
    }
    if (gpu.available && gpu.tempCelsius <= 0.0) {
        for (const DomainSensorReading &r : domainReadings) {
            if (r.domain == PhysicalDomain::CpuSoc) {
                gpu.tempCelsius = r.currentCelsius;
                break;
            }
        }
    }

    return gpu;
}

CulpritProcess CpuSocSampler::sampleTopCulpritProcess()
{
    CulpritProcess top;
    const QDir procDir(m_config.procRoot);
    if (!procDir.exists()) {
        return top;
    }

    const quint64 currentSysTicks = m_prevTotalJiffies.total;
    const quint64 deltaSysTicks = (m_prevSystemTotalTicks > 0 && currentSysTicks > m_prevSystemTotalTicks)
        ? (currentSysTicks - m_prevSystemTotalTicks)
        : 100ULL;
    m_prevSystemTotalTicks = currentSysTicks;

    const QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QHash<int, quint64> nextProcTicks;
    quint64 bestDeltaTicks = 0;

    const long pageSizeKb = std::max(4L, sysconf(_SC_PAGESIZE) / 1024L);
    const int numCpus = std::max(1, static_cast<int>(m_prevCoreJiffies.size()));

    for (const QString &entry : entries) {
        bool pidOk = false;
        const int pid = entry.toInt(&pidOk);
        if (!pidOk || pid <= 1) {
            continue;
        }

        const QString statLine = SysfsReader::readTrimmedLine(procDir.filePath(entry + QStringLiteral("/stat")));
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

        const quint64 utime = rest.at(11).toULongLong();
        const quint64 stime = rest.at(12).toULongLong();
        const quint64 totalTicks = utime + stime;
        nextProcTicks.insert(pid, totalTicks);

        if (m_prevProcTicks.contains(pid)) {
            const quint64 prevTicks = m_prevProcTicks.value(pid);
            if (totalTicks > prevTicks) {
                const quint64 dTicks = totalTicks - prevTicks;
                if (dTicks > bestDeltaTicks) {
                    bestDeltaTicks = dTicks;
                    top.pid = pid;
                    top.name = comm;
                    top.cpuPercent = std::clamp(
                        (static_cast<double>(dTicks) * 100.0 * static_cast<double>(numCpus)) / static_cast<double>(deltaSysTicks),
                        0.0,
                        100.0 * static_cast<double>(numCpus));
                    const qint64 rssPages = rest.at(21).toLongLong();
                    top.rssMegabytes = static_cast<double>(rssPages * pageSizeKb) / 1024.0;
                }
            }
        }
    }

    m_prevProcTicks = std::move(nextProcTicks);
    return top;
}

} // namespace LoofiPulse
