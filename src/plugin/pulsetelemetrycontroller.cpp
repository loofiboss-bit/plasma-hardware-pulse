#include "pulsetelemetrycontroller.h"

#include <KNotification>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

#include <algorithm>
#include <ctime>

namespace LoofiPulse
{

namespace
{

qint64 systemBoottimeMs()
{
#ifdef CLOCK_BOOTTIME
    timespec timeSpec{};
    if (clock_gettime(CLOCK_BOOTTIME, &timeSpec) == 0) {
        return static_cast<qint64>(timeSpec.tv_sec) * 1000 + timeSpec.tv_nsec / 1000000;
    }
#endif
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

PulseTelemetryController::PulseTelemetryController(QObject *parent)
    : PulseTelemetryController(QDBusConnection::systemBus(), parent)
{
}

PulseTelemetryController::PulseTelemetryController(const QDBusConnection &powerProfileBus, QObject *parent)
    : QObject(parent)
    , m_recorder(1200)
    , m_powerProfileBus(powerProfileBus)
{
    m_uptime.start();
    m_recorder.setRetentionMs(5 * 60 * 1000);
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &PulseTelemetryController::refreshNow);

    // Take two immediate initial samples so delta metrics (CPU load, PSI rate) are populated immediately
    refreshNow();
    refreshNow();
    m_timer.start();
    m_powerProfileRefreshTimer.setInterval(10000);
    connect(&m_powerProfileRefreshTimer, &QTimer::timeout, this, &PulseTelemetryController::refreshPowerProfileProperties);
    m_powerProfileRefreshTimer.start();
    refreshPowerProfileProperties();
}

void PulseTelemetryController::setPollIntervalMs(int intervalMs)
{
    const int clamped = std::clamp(intervalMs, 250, 10000);
    if (m_timer.interval() == clamped) {
        return;
    }
    m_timer.setInterval(clamped);
    Q_EMIT pollIntervalChanged();
}

void PulseTelemetryController::setNotificationsEnabled(bool enabled)
{
    if (m_notificationsEnabled == enabled) {
        return;
    }
    m_notificationsEnabled = enabled;
    Q_EMIT notificationsEnabledChanged();
}

void PulseTelemetryController::setRecordingPaused(bool paused)
{
    if (m_recordingPaused == paused) {
        return;
    }
    m_recordingPaused = paused;
    m_gapPending = true;
    setLastActionStatus(paused ? QStringLiteral("Flight Recorder paused") : QStringLiteral("Flight Recorder resumed"));
    Q_EMIT recordingPausedChanged();
}

void PulseTelemetryController::setLastActionStatus(const QString &status)
{
    setActionResult(true, status);
}

void PulseTelemetryController::setActionResult(bool succeeded, const QString &status, const QString &path)
{
    m_lastActionSucceeded = succeeded;
    m_lastActionStatus = status;
    m_lastActionPath = path;
    Q_EMIT lastActionStatusChanged();
}

QString PulseTelemetryController::batteryTimeRemainingText() const
{
    if (!m_power.batteryStateAvailable) {
        return QStringLiteral("Power state unavailable");
    }
    if (!m_power.batteryPresent) {
        return QStringLiteral("AC Desktop");
    }
    const int mins = m_power.batteryTimeRemainingMinutes;
    if (mins > 0) {
        const int h = mins / 60;
        const int m = mins % 60;
        if (h > 0) {
            return QStringLiteral("~%1h %2m").arg(h).arg(m, 2, 10, QLatin1Char('0'));
        }
        return QStringLiteral("~%1m").arg(m);
    }
    if (m_power.onAcPower) {
        return QStringLiteral("On AC (%1%)").arg(qRound(m_power.batteryPercent));
    }
    return m_power.batteryStatus.isEmpty() ? QStringLiteral("N/A") : m_power.batteryStatus;
}

void PulseTelemetryController::setSelectedHistoryIndex(int index)
{
    const int clamped = (index < 0 || index >= m_recorder.count()) ? -1 : index;
    if (m_selectedHistoryIndex == clamped) {
        return;
    }
    m_selectedHistoryIndex = clamped;
    m_selectedHistorySampleId = clamped < 0 ? 0 : m_recorder.sampleAt(clamped).sampleId;
    if (clamped < 0) {
        m_selectedHistorySnapshot.clear();
    } else {
        const FlightSample s = m_recorder.sampleAt(clamped);
        m_selectedHistorySnapshot = sampleToVariantMap(s, clamped);
        m_selectedHistorySnapshot.insert(QStringLiteral("metricMetadata"), FlightRecorder::sampleMetricMetadata(s));
    }
    Q_EMIT selectedHistoryChanged();
}

void PulseTelemetryController::setSelectedHistorySampleId(qulonglong sampleId)
{
    if (m_selectedHistorySampleId == sampleId) return;
    m_selectedHistorySampleId = sampleId;
    if (sampleId == 0) {
        m_selectedHistorySnapshot.clear();
        m_selectedHistoryIndex = -1;
    } else {
        m_selectedHistoryIndex = m_recorder.indexOfSample(sampleId);
        if (m_selectedHistoryIndex >= 0) {
            const FlightSample s = m_recorder.sampleAt(m_selectedHistoryIndex);
            m_selectedHistorySnapshot = sampleToVariantMap(s, m_selectedHistoryIndex);
            m_selectedHistorySnapshot.insert(QStringLiteral("metricMetadata"), FlightRecorder::sampleMetricMetadata(s));
        }
    }
    Q_EMIT selectedHistoryChanged();
}

QVariantMap PulseTelemetryController::sampleToVariantMap(const FlightSample &s, int index)
{
    QVariantMap m;
    m.insert(QStringLiteral("index"), index);
    m.insert(QStringLiteral("sampleId"), static_cast<qulonglong>(s.sampleId));
    m.insert(QStringLiteral("timestampMs"), s.timestampMs);
    m.insert(QStringLiteral("monotonicMs"), s.monotonicMs);
    m.insert(QStringLiteral("elapsedSincePreviousMs"), s.elapsedSincePreviousMs);
    m.insert(QStringLiteral("timeString"), QDateTime::fromMSecsSinceEpoch(s.timestampMs).toString(QStringLiteral("HH:mm:ss")));
    m.insert(QStringLiteral("cpuTemp"), s.cpuTempCelsius);
    m.insert(QStringLiteral("nvmeTemp"), s.nvmeTempCelsius);
    m.insert(QStringLiteral("wifiTemp"), s.wifiTempCelsius);
    m.insert(QStringLiteral("cpuLoad"), s.cpuLoadPercent);
    m.insert(QStringLiteral("cpuFreqMhz"), s.cpuFreqMhz);
    m.insert(QStringLiteral("gpuBusy"), s.gpuBusyPercent);
    m.insert(QStringLiteral("gpuTemp"), s.gpuTempCelsius);
    m.insert(QStringLiteral("psiCpuSome10"), s.psiCpuSome10);
    m.insert(QStringLiteral("psiMemSome10"), s.psiMemSome10);
    m.insert(QStringLiteral("psiIoFull10"), s.psiIoFull10);
    m.insert(QStringLiteral("responsivenessScore"), s.responsivenessScore);
    m.insert(QStringLiteral("systemWatts"), s.systemWatts);
    m.insert(QStringLiteral("throttled"), s.throttled);
    m.insert(QStringLiteral("severity"), static_cast<int>(s.severity));
    m.insert(QStringLiteral("verdictHeadline"), s.verdictHeadline);
    m.insert(QStringLiteral("culpritName"), s.culpritName);
    m.insert(QStringLiteral("culpritPid"), s.culpritPid);
    m.insert(QStringLiteral("culpritCpuPercent"), s.culpritCpuPercent);
    m.insert(QStringLiteral("culpritRssMb"), s.culpritRssMb);
    m.insert(QStringLiteral("metricQuality"), s.metricQuality);
    for (auto it = s.metricQuality.cbegin(); it != s.metricQuality.cend(); ++it) {
        m.insert(it.key(), it.value());
    }
    return m;
}

QVariantMap PulseTelemetryController::selectedHistoryDetail() const
{
    if (m_selectedHistorySampleId != 0) {
        const int idx = m_recorder.indexOfSample(m_selectedHistorySampleId);
        if (idx >= 0) {
            const FlightSample s = m_recorder.sampleAt(idx);
            QVariantMap detail = sampleToVariantMap(s, idx);
            detail.insert(QStringLiteral("metricMetadata"), FlightRecorder::sampleMetricMetadata(s));
            return detail;
        }
        return m_selectedHistorySnapshot;
    }
    const int idx = m_recorder.count() - 1;
    if (idx < 0) {
        return {};
    }
    const FlightSample s = m_recorder.sampleAt(idx);
    QVariantMap detail = sampleToVariantMap(s, idx);
    detail.insert(QStringLiteral("metricMetadata"), FlightRecorder::sampleMetricMetadata(s));
    return detail;
}

QVariantMap PulseTelemetryController::metricMetadata() const
{
    const qint64 sampledAt = m_lastSampleWallTimeMs;
    const auto metric = [sampledAt](const QString &state, const QString &source, const QString &detail = QString()) {
        return QVariantMap{{QStringLiteral("state"), state},
                           {QStringLiteral("source"), source},
                           {QStringLiteral("detail"), detail},
                           {QStringLiteral("sampledAtMs"), sampledAt}};
    };

    QVariantMap result;
    result.insert(QStringLiteral("cpuLoad"), metric(!m_cpu.loadAvailable ? QStringLiteral("unavailable") : (m_cpu.loadWarmedUp ? QStringLiteral("available") : QStringLiteral("warming-up")),
                                                      QStringLiteral("/proc/stat"),
                                                      m_cpu.loadAvailable ? QString() : QStringLiteral("CPU counters could not be read.")));
    result.insert(QStringLiteral("cpuTemperature"), metric(m_cpu.temperatureAvailable ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                              QStringLiteral("/sys/class/hwmon temp*_input"),
                                                              m_cpu.temperatureAvailable ? QString() : QStringLiteral("No CPU temperature sensor was found.")));
    result.insert(QStringLiteral("thermalHeadroom"), metric(thermalHeadroomAvailable() ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                              QStringLiteral("/sys/class/hwmon temp*_max or temp*_crit")));
    result.insert(QStringLiteral("gpuLoad"), metric(m_gpu.busyAvailable ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                       QStringLiteral("/sys/class/drm device/gpu_busy_percent"),
                                                       m_gpu.busyAvailable ? QString() : QStringLiteral("This GPU does not expose a supported busy counter.")));
    result.insert(QStringLiteral("gpuTemperature"), metric(m_gpu.temperatureAvailable ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                             QStringLiteral("GPU device hwmon temp*_input"),
                                                             m_gpu.temperatureAvailable ? QString() : QStringLiteral("No temperature sensor was matched to the selected GPU.")));
    result.insert(QStringLiteral("cpuPressure"), metric(m_psiMem.cpuPsi.someAvailable ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                           QStringLiteral("/proc/pressure/cpu")));
    result.insert(QStringLiteral("memoryPressure"), metric(m_psiMem.memoryPsi.someAvailable ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                              QStringLiteral("/proc/pressure/memory")));
    result.insert(QStringLiteral("ioPressure"), metric(m_psiMem.ioPsi.someAvailable ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                         QStringLiteral("/proc/pressure/io")));
    result.insert(QStringLiteral("pressureIndex"), metric(m_psiMem.responsivenessAvailable ? QStringLiteral("calculated") : QStringLiteral("unavailable"),
                                                            QStringLiteral("Calculated from PSI avg10 values"),
                                                            QStringLiteral("This index is not a measured response time.")));
    result.insert(QStringLiteral("memoryUsage"), metric(m_psiMem.ramAvailable ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                          QStringLiteral("/proc/meminfo MemTotal and MemAvailable")));
    result.insert(QStringLiteral("zram"), metric(m_psiMem.zramActive ? QStringLiteral("available") : QStringLiteral("not-active"),
                                                    QStringLiteral("/sys/block/zram*/mm_stat")));
    result.insert(QStringLiteral("packagePower"), metric(m_power.raplExactAvailable ? QStringLiteral("available")
                                                                 : (m_power.packagePowerEstimated ? QStringLiteral("estimated") : QStringLiteral("unavailable")),
                                                           m_power.raplExactAvailable ? QStringLiteral("Intel RAPL energy_uj")
                                                                                      : QStringLiteral("Estimated from CPU load and frequency")));
    result.insert(QStringLiteral("fanSpeed"), metric(m_power.fanAvailable ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                       QStringLiteral("/sys/class/hwmon fan*_input")));
    result.insert(QStringLiteral("battery"), metric(!m_power.batteryStateAvailable ? QStringLiteral("unavailable")
                                                                 : (m_power.batteryPresent ? QStringLiteral("available") : QStringLiteral("not-present")),
                                                       QStringLiteral("/sys/class/power_supply")));
    result.insert(QStringLiteral("powerProfile"), metric(m_powerProfileServiceAvailable ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                            QStringLiteral("net.hadess.PowerProfiles D-Bus")));
    return result;
}

void PulseTelemetryController::refreshNow()
{
    const qint64 sampleBoottimeMs = systemBoottimeMs();
    const qint64 sampleWallTimeMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsedMs = m_lastBoottimeMs > 0
        ? std::max<qint64>(100, sampleBoottimeMs - m_lastBoottimeMs)
        : 1000;
    const qint64 wallElapsedMs = m_lastSampleWallTimeMs > 0 ? sampleWallTimeMs - m_lastSampleWallTimeMs : 0;
    const qint64 gapThresholdMs = std::max(1500, m_timer.interval() * 2);
    m_gapPending = m_gapPending || elapsedMs > gapThresholdMs || wallElapsedMs > gapThresholdMs;
    m_lastBoottimeMs = sampleBoottimeMs;
    m_lastSampleWallTimeMs = sampleWallTimeMs;

    m_domains = m_sysfsReader.sampleHwmonDomains(elapsedMs);
    m_cpu = m_cpuSampler.sampleCpu(m_domains, elapsedMs);
    m_gpu = m_cpuSampler.sampleGpu(m_domains);
    m_psiMem = m_psiSampler.sample(elapsedMs);
    const double validCpuLoad = cpuLoadAvailable() ? m_cpu.totalLoadPercent : -1.0;
    m_power = m_powerSampler.sample(elapsedMs, validCpuLoad, m_cpu.avgFreqMhz, m_cpu.maxFreqMhz);
    m_power.activePowerProfile = m_activePowerProfile;
    m_power.availablePowerProfiles = m_availablePowerProfiles;
    const bool elevatedActivity = (m_cpu.loadAvailable && m_cpu.totalLoadPercent >= 35.0)
        || m_cpu.deltaThrottleEvents > 0
        || (m_psiMem.responsivenessAvailable && m_psiMem.responsivenessScore < 92.0)
        || m_psiMem.memoryPsi.someAvg10 >= 2.0
        || m_psiMem.ioPsi.fullAvg10 >= 4.0;
    const bool shouldScanProcesses = m_processScanWarmupCount < 2
        || elevatedActivity
        || m_lastProcessScanBoottimeMs == 0
        || (sampleBoottimeMs - m_lastProcessScanBoottimeMs) >= 2500;
    if (shouldScanProcesses) {
        m_processRadar = m_cpuSampler.sampleProcessRadar(5);
        m_lastProcessScanBoottimeMs = sampleBoottimeMs;
        if (m_processScanWarmupCount < 2) {
            ++m_processScanWarmupCount;
        }
        m_topProcessesVariant.clear();
        for (const CulpritProcess &p : std::as_const(m_processRadar.topProcesses)) {
            QVariantMap pm;
            pm.insert(QStringLiteral("pid"), p.pid);
            pm.insert(QStringLiteral("name"), p.name);
            pm.insert(QStringLiteral("cpuPercent"), p.cpuPercent);
            pm.insert(QStringLiteral("rssMegabytes"), p.rssMegabytes);
            m_topProcessesVariant.append(pm);
        }
    }

    const CausalVerdict candidateVerdict = CausalDiagnosisEngine::evaluate(
        m_cpu,
        m_gpu,
        m_psiMem,
        m_power,
        m_domains,
        m_processRadar.topCpuProcess,
        m_processRadar.topMemoryProcess);
    m_verdict = m_verdictStabilizer.update(candidateVerdict, m_uptime.elapsed());

    m_nvmeTemp = 0.0;
    m_wifiTemp = 0.0;
    m_nvmeTempAvailable = false;
    m_wifiTempAvailable = false;
    m_domainSensorsVariant.clear();

    for (const DomainSensorReading &d : std::as_const(m_domains)) {
        if (d.domain == PhysicalDomain::StorageNvme && d.available
            && (!m_nvmeTempAvailable || d.currentCelsius > m_nvmeTemp)) {
            m_nvmeTemp = d.currentCelsius;
            m_nvmeTempAvailable = true;
        } else if (d.domain == PhysicalDomain::NetworkWireless && d.available
                   && (!m_wifiTempAvailable || d.currentCelsius > m_wifiTemp)) {
            m_wifiTemp = d.currentCelsius;
            m_wifiTempAvailable = true;
        }

        QVariantMap dm;
        dm.insert(QStringLiteral("id"), d.id);
        dm.insert(QStringLiteral("chipName"), d.chipName);
        dm.insert(QStringLiteral("label"), d.label);
        dm.insert(QStringLiteral("domainId"), static_cast<int>(d.domain));
        dm.insert(QStringLiteral("domainName"), SysfsReader::domainDisplayName(d.domain));
        dm.insert(QStringLiteral("currentCelsius"), d.currentCelsius);
        dm.insert(QStringLiteral("available"), d.available);
        dm.insert(QStringLiteral("maxCelsius"), d.maxCelsius);
        dm.insert(QStringLiteral("limitsAvailable"), d.limitsAvailable);
        dm.insert(QStringLiteral("headroomCelsius"), d.headroomCelsius);
        dm.insert(QStringLiteral("velocityCelsiusPerSec"), d.velocityCelsiusPerSec);
        dm.insert(QStringLiteral("sysfsPath"), d.sysfsPath);
        m_domainSensorsVariant.append(dm);
    }

    m_coreItemsVariant.clear();
    for (const CoreTelemetry &c : std::as_const(m_cpu.cores)) {
        QVariantMap cm;
        cm.insert(QStringLiteral("coreId"), c.coreId);
        cm.insert(QStringLiteral("loadAvailable"), c.loadAvailable);
        cm.insert(QStringLiteral("loadPercent"), c.loadPercent);
        cm.insert(QStringLiteral("currentFreqMhz"), c.currentFreqMhz);
        cm.insert(QStringLiteral("maxFreqMhz"), c.maxFreqMhz);
        cm.insert(QStringLiteral("tempCelsius"), c.tempCelsius);
        cm.insert(QStringLiteral("throttleCount"), c.throttleCount);
        cm.insert(QStringLiteral("throttleCountersAvailable"), m_cpu.throttleCountersAvailable);
        m_coreItemsVariant.append(cm);
    }

    if (!m_recordingPaused || m_recorder.count() == 0) {
        const CulpritProcess &primaryCulprit = (m_verdict.primaryBottleneckDomain.contains(QStringLiteral("Memory")))
            ? m_verdict.topMemoryProcess
            : m_verdict.topProcess;

        FlightSample sample;
        sample.timestampMs = sampleWallTimeMs;
        sample.monotonicMs = sampleBoottimeMs;
        sample.elapsedSincePreviousMs = elapsedMs;
        sample.cpuTempCelsius = m_cpu.packageTempCelsius;
        sample.nvmeTempCelsius = m_nvmeTemp;
        sample.wifiTempCelsius = m_wifiTemp;
        sample.cpuLoadPercent = m_cpu.totalLoadPercent;
        sample.cpuFreqMhz = m_cpu.avgFreqMhz;
        sample.gpuBusyPercent = m_gpu.busyPercent;
        sample.gpuTempCelsius = m_gpu.tempCelsius;
        sample.gpuFreqMhz = m_gpu.currentFreqMhz;
        sample.gpuPowerWatts = m_gpu.powerWatts;
        sample.gpuVramUsedMegabytes = m_gpu.vramUsedMegabytes;
        sample.gpuVramTotalMegabytes = m_gpu.vramTotalMegabytes;
        sample.psiCpuSome10 = m_psiMem.cpuPsi.someAvg10;
        sample.psiCpuSome60 = m_psiMem.cpuPsi.someAvg60;
        sample.psiMemSome10 = m_psiMem.memoryPsi.someAvg10;
        sample.psiMemSome60 = m_psiMem.memoryPsi.someAvg60;
        sample.psiMemFull10 = m_psiMem.memoryPsi.fullAvg10;
        sample.psiMemFull60 = m_psiMem.memoryPsi.fullAvg60;
        sample.psiIoSome10 = m_psiMem.ioPsi.someAvg10;
        sample.psiIoSome60 = m_psiMem.ioPsi.someAvg60;
        sample.psiIoFull10 = m_psiMem.ioPsi.fullAvg10;
        sample.psiIoFull60 = m_psiMem.ioPsi.fullAvg60;
        sample.psiCpuStallMsPerSec = m_psiMem.cpuPsi.deltaStallMsPerSec;
        sample.psiMemStallMsPerSec = m_psiMem.memoryPsi.deltaStallMsPerSec;
        sample.psiIoStallMsPerSec = m_psiMem.ioPsi.deltaStallMsPerSec;
        sample.responsivenessScore = m_psiMem.responsivenessScore;
        const bool batteryPowerAvailable = m_power.batteryPresent && m_power.batteryPowerAvailable
            && m_power.batteryStateAvailable && !m_power.onAcPower;
        sample.systemWatts = batteryPowerAvailable ? m_power.batteryWatts : m_power.packageWatts;
        sample.packageWatts = m_power.packageWatts;
        sample.batteryWatts = m_power.batteryWatts;
        sample.batteryPercent = m_power.batteryPercent;
        sample.batteryHealthPercent = m_power.batteryHealthPercent;
        sample.pl1LimitWatts = m_power.pl1LimitWatts;
        sample.pl2LimitWatts = m_power.pl2LimitWatts;
        sample.fanRpm = m_power.primaryFanRpm;
        sample.ramUsedPercent = m_psiMem.ramUsedPercent;
        sample.ramUsedBytes = m_psiMem.ramUsedBytes;
        sample.ramTotalBytes = m_psiMem.ramTotalBytes;
        sample.swapUsedBytes = m_psiMem.swapTotalBytes > m_psiMem.swapFreeBytes
            ? m_psiMem.swapTotalBytes - m_psiMem.swapFreeBytes : 0;
        sample.swapTotalBytes = m_psiMem.swapTotalBytes;
        sample.zramCompressionRatio = m_psiMem.zramCompressionRatio;
        sample.zramOriginalBytes = m_psiMem.zramOrigBytes;
        sample.zramCompressedBytes = m_psiMem.zramCompressedBytes;
        sample.zramMemoryUsedBytes = m_psiMem.zramMemUsedTotalBytes;
        sample.throttleEventCount = m_cpu.totalThrottleEvents;
        sample.newThrottleEvents = m_cpu.deltaThrottleEvents;
        sample.batteryPresent = m_power.batteryPresent;
        sample.onAcPower = m_power.onAcPower;
        sample.activePowerProfile = m_activePowerProfile;
        sample.throttled = (m_cpu.deltaThrottleEvents > 0);
        sample.severity = m_verdict.severity;
        sample.verdictHeadline = m_verdict.headline;
        sample.culpritName = primaryCulprit.name;
        sample.culpritPid = primaryCulprit.pid;
        sample.culpritCpuPercent = primaryCulprit.cpuPercent;
        sample.culpritRssMb = primaryCulprit.rssMegabytes;
        sample.metricQuality.insert(QStringLiteral("cpuLoadAvailable"), cpuLoadAvailable());
        sample.metricQuality.insert(QStringLiteral("cpuLoadWarmedUp"), m_cpu.loadWarmedUp);
        sample.metricQuality.insert(QStringLiteral("cpuFrequencyAvailable"), m_cpu.frequencyAvailable);
        sample.metricQuality.insert(QStringLiteral("cpuTempAvailable"), m_cpu.temperatureAvailable);
        sample.metricQuality.insert(QStringLiteral("thermalHeadroomAvailable"), m_cpu.temperatureAvailable && m_cpu.thermalLimitAvailable);
        sample.metricQuality.insert(QStringLiteral("gpuBusyAvailable"), m_gpu.busyAvailable);
        sample.metricQuality.insert(QStringLiteral("gpuFrequencyAvailable"), m_gpu.frequencyAvailable);
        sample.metricQuality.insert(QStringLiteral("gpuTempAvailable"), m_gpu.temperatureAvailable);
        sample.metricQuality.insert(QStringLiteral("gpuPowerAvailable"), m_gpu.powerAvailable);
        sample.metricQuality.insert(QStringLiteral("gpuVramAvailable"), m_gpu.vramAvailable);
        sample.metricQuality.insert(QStringLiteral("nvmeTempAvailable"), m_nvmeTempAvailable);
        sample.metricQuality.insert(QStringLiteral("wifiTempAvailable"), m_wifiTempAvailable);
        sample.metricQuality.insert(QStringLiteral("psiAvailable"), m_psiMem.responsivenessAvailable);
        sample.metricQuality.insert(QStringLiteral("psiCpuSomeAvailable"), m_psiMem.cpuPsi.someAvailable);
        sample.metricQuality.insert(QStringLiteral("psiMemorySomeAvailable"), m_psiMem.memoryPsi.someAvailable);
        sample.metricQuality.insert(QStringLiteral("psiMemoryFullAvailable"), m_psiMem.memoryPsi.fullAvailable);
        sample.metricQuality.insert(QStringLiteral("psiIoSomeAvailable"), m_psiMem.ioPsi.someAvailable);
        sample.metricQuality.insert(QStringLiteral("psiIoFullAvailable"), m_psiMem.ioPsi.fullAvailable);
        sample.metricQuality.insert(QStringLiteral("ramAvailable"), m_psiMem.ramAvailable);
        sample.metricQuality.insert(QStringLiteral("swapAvailable"), m_psiMem.swapAvailable);
        sample.metricQuality.insert(QStringLiteral("zramAvailable"), m_psiMem.zramAvailable);
        sample.metricQuality.insert(QStringLiteral("zramCompressionAvailable"), m_psiMem.zramCompressionAvailable);
        sample.metricQuality.insert(QStringLiteral("packagePowerAvailable"), m_power.packagePowerAvailable);
        sample.metricQuality.insert(QStringLiteral("packagePowerEstimated"), m_power.packagePowerEstimated);
        sample.metricQuality.insert(QStringLiteral("systemPowerEstimated"), !batteryPowerAvailable && m_power.packagePowerEstimated);
        sample.metricQuality.insert(QStringLiteral("systemPowerAvailable"), batteryPowerAvailable || m_power.packagePowerAvailable);
        sample.metricQuality.insert(QStringLiteral("batteryPowerAvailable"), m_power.batteryPowerAvailable);
        sample.metricQuality.insert(QStringLiteral("batteryAvailable"), m_power.batteryStateAvailable && m_power.batteryPresent);
        sample.metricQuality.insert(QStringLiteral("powerStateAvailable"), m_power.batteryStateAvailable);
        sample.metricQuality.insert(QStringLiteral("batteryLevelAvailable"), m_power.batteryLevelAvailable);
        sample.metricQuality.insert(QStringLiteral("batteryHealthAvailable"), m_power.batteryHealthAvailable);
        sample.metricQuality.insert(QStringLiteral("powerLimitsAvailable"), m_power.pl1LimitWatts > 0.0 || m_power.pl2LimitWatts > 0.0);
        sample.metricQuality.insert(QStringLiteral("throttleCountersAvailable"), m_cpu.throttleCountersAvailable);
        sample.metricQuality.insert(QStringLiteral("fanAvailable"), m_power.fanAvailable);
        sample.metricQuality.insert(QStringLiteral("gapBefore"), m_recorder.count() > 0 ? m_gapPending : false);

        if (m_selectedHistorySampleId != 0) {
            const int selectedIndex = m_recorder.indexOfSample(m_selectedHistorySampleId);
            if (selectedIndex >= 0) {
                const FlightSample sel = m_recorder.sampleAt(selectedIndex);
                m_selectedHistorySnapshot = sampleToVariantMap(sel, selectedIndex);
                m_selectedHistorySnapshot.insert(QStringLiteral("metricMetadata"), FlightRecorder::sampleMetricMetadata(sel));
            }
        }
        sample.sampleId = m_recorder.appendSample(sample);
        m_gapPending = false;
        const int latestIdx = m_recorder.count() - 1;
        m_historySamplesVariant.append(sampleToVariantMap(m_recorder.sampleAt(latestIdx), latestIdx));
        while (m_historySamplesVariant.size() > m_recorder.count()) {
            m_historySamplesVariant.removeFirst();
        }
        if (m_recorder.count() > 0
            && (m_historySamplesVariant.size() != m_recorder.count()
                || m_historySamplesVariant.first().toMap().value(QStringLiteral("sampleId")).toULongLong() != m_recorder.sampleAt(0).sampleId)) {
            m_historySamplesVariant.clear();
            for (int i = 0; i < m_recorder.count(); ++i) {
                m_historySamplesVariant.append(sampleToVariantMap(m_recorder.sampleAt(i), i));
            }
        }
        m_selectedHistoryIndex = m_recorder.indexOfSample(m_selectedHistorySampleId);
    }

    maybeEmitAlertNotifications();

    Q_EMIT telemetryUpdated();
    Q_EMIT selectedHistoryChanged();
}

void PulseTelemetryController::maybeEmitAlertNotifications()
{
    if (!m_notificationsEnabled || m_verdict.severity == VerdictSeverity::Unknown) {
        return;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastAlertTimestampMs < 60000) {
        return; // 60s cooldown between alert popups
    }

    if (m_verdict.severity == VerdictSeverity::CriticalThrottle) {
        m_lastAlertTimestampMs = nowMs;
        auto *notif = new KNotification(QStringLiteral("thermalThrottle"), KNotification::CloseOnTimeout, this);
        notif->setComponentName(QStringLiteral("plasma_applet_com.github.loofi.hardwarepulse"));
        notif->setTitle(m_verdict.headline);
        notif->setText(m_verdict.causalChain);
        notif->sendEvent();
    } else if (m_verdict.severity == VerdictSeverity::Bottleneck && m_psiMem.responsivenessScore < 85.0) {
        m_lastAlertTimestampMs = nowMs;
        auto *notif = new KNotification(QStringLiteral("psiBottleneck"), KNotification::CloseOnTimeout, this);
        notif->setComponentName(QStringLiteral("plasma_applet_com.github.loofi.hardwarepulse"));
        notif->setTitle(m_verdict.headline);
        notif->setText(m_verdict.causalChain);
        notif->sendEvent();
    }
}

bool PulseTelemetryController::setPowerProfile(const QString &profile)
{
    if (m_powerProfileChangePending) {
        setActionResult(false, QStringLiteral("A power profile change is already in progress."));
        return false;
    }
    if (!m_powerProfileServiceAvailable || !m_availablePowerProfiles.contains(profile)) {
        setActionResult(false, QStringLiteral("That power profile is not available on this system."));
        return false;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("net.hadess.PowerProfiles"),
        QStringLiteral("/net/hadess/PowerProfiles"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Set"));
    message << QStringLiteral("net.hadess.PowerProfiles")
            << QStringLiteral("ActiveProfile")
            << QVariant::fromValue(QDBusVariant(profile));
    m_powerProfileChangePending = true;
    setActionResult(false, QStringLiteral("Changing power profile to '%1'…").arg(profile));
    auto *watcher = new QDBusPendingCallWatcher(m_powerProfileBus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, profile]() {
        const QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            m_powerProfileChangePending = false;
            setActionResult(false, QStringLiteral("Power profile change failed: %1").arg(reply.error().message()));
            return;
        }

        QDBusMessage verify = QDBusMessage::createMethodCall(
            QStringLiteral("net.hadess.PowerProfiles"),
            QStringLiteral("/net/hadess/PowerProfiles"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("Get"));
        verify << QStringLiteral("net.hadess.PowerProfiles") << QStringLiteral("ActiveProfile");
        auto *verifyWatcher = new QDBusPendingCallWatcher(m_powerProfileBus.asyncCall(verify), this);
        connect(verifyWatcher, &QDBusPendingCallWatcher::finished, this, [this, verifyWatcher, profile]() {
            const QDBusPendingReply<QDBusVariant> verified = *verifyWatcher;
            verifyWatcher->deleteLater();
            m_powerProfileChangePending = false;
            if (verified.isError()) {
                setActionResult(false, QStringLiteral("The profile change was sent, but its result could not be verified."));
                return;
            }
            const QString active = verified.value().variant().toString();
            if (active != profile) {
                setActionResult(false, QStringLiteral("Power Profiles did not activate '%1'. Current profile: '%2'.").arg(profile, active));
                return;
            }
            m_activePowerProfile = active;
            setActionResult(true, QStringLiteral("Power profile changed to '%1'.").arg(active));
            Q_EMIT telemetryUpdated();
        });
    });
    return true;
}

void PulseTelemetryController::refreshPowerProfileProperties()
{
    if (!m_powerProfileBus.isConnected()) {
        m_powerProfileServiceAvailable = false;
        m_availablePowerProfiles.clear();
        Q_EMIT telemetryUpdated();
        return;
    }

    const auto requestProperty = [this](const QString &property, auto callback) {
        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("net.hadess.PowerProfiles"),
            QStringLiteral("/net/hadess/PowerProfiles"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("Get"));
        message << QStringLiteral("net.hadess.PowerProfiles") << property;
        auto *watcher = new QDBusPendingCallWatcher(m_powerProfileBus.asyncCall(message), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, callback]() {
            const QDBusPendingReply<QDBusVariant> reply = *watcher;
            watcher->deleteLater();
            callback(reply);
        });
    };

    requestProperty(QStringLiteral("ActiveProfile"), [this](const QDBusPendingReply<QDBusVariant> &reply) {
        if (reply.isError()) {
            m_powerProfileServiceAvailable = false;
            m_activePowerProfile.clear();
            m_availablePowerProfiles.clear();
            Q_EMIT telemetryUpdated();
            return;
        }
        const QString active = reply.value().variant().toString().trimmed();
        if (!active.isEmpty()) {
            m_activePowerProfile = active;
            m_powerProfileServiceAvailable = true;
            Q_EMIT telemetryUpdated();
        }
    });

    requestProperty(QStringLiteral("Profiles"), [this](const QDBusPendingReply<QDBusVariant> &reply) {
        if (reply.isError()) {
            m_powerProfileServiceAvailable = false;
            m_availablePowerProfiles.clear();
            Q_EMIT telemetryUpdated();
            return;
        }
        QStringList profiles;
        const QVariant value = reply.value().variant();
        if (value.canConvert<QStringList>()) {
            profiles = value.toStringList();
        } else if (value.canConvert<QVariantList>()) {
            for (const QVariant &entry : value.toList()) {
                if (entry.canConvert<QVariantMap>()) profiles.append(entry.toMap().value(QStringLiteral("Profile")).toString());
                else profiles.append(entry.toString());
            }
        } else if (value.canConvert<QDBusArgument>()) {
            const QDBusArgument argument = value.value<QDBusArgument>();
            if (argument.currentSignature() == QStringLiteral("as")) {
                argument >> profiles;
            } else if (argument.currentSignature() == QStringLiteral("aa{sv}")) {
                argument.beginArray();
                while (!argument.atEnd()) {
                    QVariantMap properties;
                    argument >> properties;
                    QVariant nameValue = properties.value(QStringLiteral("Profile"));
                    if (nameValue.canConvert<QDBusVariant>()) {
                        nameValue = nameValue.value<QDBusVariant>().variant();
                    }
                    const QString name = nameValue.toString().trimmed();
                    if (!name.isEmpty()) profiles.append(name);
                }
                argument.endArray();
            }
        }
        profiles.removeDuplicates();
        profiles.removeAll(QString());
        m_availablePowerProfiles = profiles;
        Q_EMIT telemetryUpdated();
    });
}

bool PulseTelemetryController::openSystemMonitor()
{
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("plasma-systemmonitor"));
    if (executable.isEmpty()) {
        setActionResult(false, QStringLiteral("KDE System Monitor is not installed."));
        return false;
    }
    if (!QProcess::startDetached(executable, {QStringLiteral("--page-id"), QStringLiteral("processes.page")})) {
        setActionResult(false, QStringLiteral("KDE System Monitor could not be started."));
        return false;
    }
    setActionResult(true, QStringLiteral("Opened KDE System Monitor."));
    return true;
}

bool PulseTelemetryController::openReportsFolder()
{
    if (m_reportsDirectory.isEmpty() || !QDir(m_reportsDirectory).exists()) {
        setActionResult(false, QStringLiteral("The reports folder is not available yet."));
        return false;
    }
    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(m_reportsDirectory));
    setActionResult(opened, opened ? QStringLiteral("Opened the reports folder.")
                                   : QStringLiteral("The reports folder could not be opened."), m_reportsDirectory);
    return opened;
}

void PulseTelemetryController::launchStudioApp()
{
    QString exe = QStandardPaths::findExecutable(QStringLiteral("loofi-pulse"));
    if (exe.isEmpty()) {
        const QString sibling = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("loofi-pulse"));
        if (QFileInfo::exists(sibling)) {
            exe = sibling;
        }
    }
    if (!exe.isEmpty()) {
        const bool started = QProcess::startDetached(exe, {});
        setActionResult(started, started ? QStringLiteral("Loofi Pulse opened in a new window.")
                                        : QStringLiteral("Loofi Pulse could not be started."));
    } else {
        setActionResult(false, QStringLiteral("Loofi Pulse could not be found on this system."));
    }
}

int PulseTelemetryController::jumpToPeakThermalSpike()
{
    const int idx = m_recorder.peakTemperatureIndex();
    if (idx >= 0) {
        setSelectedHistorySampleId(m_recorder.sampleAt(idx).sampleId);
    }
    return idx;
}

int PulseTelemetryController::jumpToWorstStallSpike()
{
    const int idx = m_recorder.worstResponsivenessIndex();
    if (idx >= 0) {
        setSelectedHistorySampleId(m_recorder.sampleAt(idx).sampleId);
    }
    return idx;
}

QString PulseTelemetryController::copyIncidentReportToClipboard()
{
    const QString md = m_recorder.exportSummaryMarkdown();
    if (QClipboard *cb = QGuiApplication::clipboard()) {
        cb->setText(md);
        setActionResult(true, QStringLiteral("Copied incident report to clipboard."));
    } else {
        setActionResult(false, QStringLiteral("Clipboard is unavailable."));
    }
    return md;
}

QString PulseTelemetryController::exportIncidentReportToFile()
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
        setActionResult(false, QStringLiteral("No writable application data folder is available."));
        return {};
    }
    const QString reportsDir = QDir(baseDir).filePath(QStringLiteral("reports"));
    if (!QDir().mkpath(reportsDir) && !QDir(reportsDir).exists()) {
        setActionResult(false, QStringLiteral("The reports folder could not be created."));
        return {};
    }
    m_reportsDirectory = reportsDir;

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString mdPath = QDir(reportsDir).filePath(QStringLiteral("loofi-pulse-report-%1.md").arg(stamp));
    const QString jsonPath = QDir(reportsDir).filePath(QStringLiteral("loofi-pulse-samples-%1.json").arg(stamp));

    const auto writeAtomically = [](const QString &path, const QString &contents, QString *error) {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            *error = file.errorString();
            return false;
        }
        const QByteArray bytes = contents.toUtf8();
        if (file.write(bytes) != bytes.size()) {
            *error = file.errorString();
            file.cancelWriting();
            return false;
        }
        if (!file.commit()) {
            *error = file.errorString();
            return false;
        }
        return true;
    };

    QString mdError;
    QString jsonError;
    const bool markdownSaved = writeAtomically(mdPath, m_recorder.exportSummaryMarkdown(), &mdError);
    const bool jsonSaved = writeAtomically(jsonPath, m_recorder.exportSamplesJson(), &jsonError);
    if (markdownSaved && jsonSaved) {
        setActionResult(true, QStringLiteral("Saved Markdown and JSON reports."), mdPath);
        return mdPath;
    }
    if (markdownSaved) {
        setActionResult(false, QStringLiteral("Markdown report saved; JSON report failed: %1").arg(jsonError), mdPath);
        return mdPath;
    }
    if (jsonSaved) {
        setActionResult(false, QStringLiteral("JSON report saved; Markdown report failed: %1").arg(mdError), jsonPath);
        return jsonPath;
    }
    setActionResult(false, QStringLiteral("Report export failed. Markdown: %1. JSON: %2").arg(mdError, jsonError));
    return {};
}

} // namespace LoofiPulse
