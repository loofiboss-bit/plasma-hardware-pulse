#pragma once

#include "causaldiagnosisengine.h"
#include "cpusocsampler.h"
#include "flightrecorder.h"
#include "powerflowsampler.h"
#include "psimemorysampler.h"
#include "sysfsreader.h"
#include "telemetrytypes.h"

#include <QElapsedTimer>
#include <QDBusConnection>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <qqmlintegration.h>

namespace LoofiPulse
{

class PulseTelemetryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // Core CPU & Thermal properties
    Q_PROPERTY(double cpuLoadPercent READ cpuLoadPercent NOTIFY telemetryUpdated)
    Q_PROPERTY(double cpuPackageTemp READ cpuPackageTemp NOTIFY telemetryUpdated)
    Q_PROPERTY(double thermalHeadroom READ thermalHeadroom NOTIFY telemetryUpdated)
    Q_PROPERTY(double thermalVelocity READ thermalVelocity NOTIFY telemetryUpdated)
    Q_PROPERTY(int cpuAvgFreqMhz READ cpuAvgFreqMhz NOTIFY telemetryUpdated)
    Q_PROPERTY(int cpuMaxFreqMhz READ cpuMaxFreqMhz NOTIFY telemetryUpdated)
    Q_PROPERTY(int cpuPeakFreqMhz READ cpuPeakFreqMhz NOTIFY telemetryUpdated)
    Q_PROPERTY(qint64 totalThrottleEvents READ totalThrottleEvents NOTIFY telemetryUpdated)
    Q_PROPERTY(qint64 deltaThrottleEvents READ deltaThrottleEvents NOTIFY telemetryUpdated)
    Q_PROPERTY(bool cpuLoadAvailable READ cpuLoadAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool cpuTempAvailable READ cpuTempAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool thermalHeadroomAvailable READ thermalHeadroomAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool throttleCountersAvailable READ throttleCountersAvailable NOTIFY telemetryUpdated)

    // GPU properties
    Q_PROPERTY(bool gpuAvailable READ gpuAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(QString gpuDriverName READ gpuDriverName NOTIFY telemetryUpdated)
    Q_PROPERTY(int gpuCurrentFreqMhz READ gpuCurrentFreqMhz NOTIFY telemetryUpdated)
    Q_PROPERTY(int gpuBoostFreqMhz READ gpuBoostFreqMhz NOTIFY telemetryUpdated)
    Q_PROPERTY(double gpuBusyPercent READ gpuBusyPercent NOTIFY telemetryUpdated)
    Q_PROPERTY(double gpuTempCelsius READ gpuTempCelsius NOTIFY telemetryUpdated)
    Q_PROPERTY(double gpuVramUsedMib READ gpuVramUsedMib NOTIFY telemetryUpdated)
    Q_PROPERTY(double gpuVramTotalMib READ gpuVramTotalMib NOTIFY telemetryUpdated)
    Q_PROPERTY(double gpuPowerWatts READ gpuPowerWatts NOTIFY telemetryUpdated)
    Q_PROPERTY(bool gpuFrequencyAvailable READ gpuFrequencyAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool gpuBusyAvailable READ gpuBusyAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool gpuTempAvailable READ gpuTempAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool gpuPowerAvailable READ gpuPowerAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool gpuVramAvailable READ gpuVramAvailable NOTIFY telemetryUpdated)

    // PSI & ZRAM / Memory properties
    Q_PROPERTY(double psiCpuSome10 READ psiCpuSome10 NOTIFY telemetryUpdated)
    Q_PROPERTY(double psiMemSome10 READ psiMemSome10 NOTIFY telemetryUpdated)
    Q_PROPERTY(double psiMemFull10 READ psiMemFull10 NOTIFY telemetryUpdated)
    Q_PROPERTY(double psiIoSome10 READ psiIoSome10 NOTIFY telemetryUpdated)
    Q_PROPERTY(double psiIoFull10 READ psiIoFull10 NOTIFY telemetryUpdated)
    Q_PROPERTY(double psiCpuStallMs READ psiCpuStallMs NOTIFY telemetryUpdated)
    Q_PROPERTY(double psiMemStallMs READ psiMemStallMs NOTIFY telemetryUpdated)
    Q_PROPERTY(double psiIoStallMs READ psiIoStallMs NOTIFY telemetryUpdated)
    Q_PROPERTY(double responsivenessScore READ responsivenessScore NOTIFY telemetryUpdated)
    Q_PROPERTY(bool psiAvailable READ psiAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool psiCpuAvailable READ psiCpuAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool psiMemoryAvailable READ psiMemoryAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool psiMemoryFullAvailable READ psiMemoryFullAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool psiIoAvailable READ psiIoAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool psiIoFullAvailable READ psiIoFullAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool memoryAvailable READ memoryAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(double ramUsedPercent READ ramUsedPercent NOTIFY telemetryUpdated)
    Q_PROPERTY(double ramUsedGib READ ramUsedGib NOTIFY telemetryUpdated)
    Q_PROPERTY(double ramTotalGib READ ramTotalGib NOTIFY telemetryUpdated)
    Q_PROPERTY(bool zramActive READ zramActive NOTIFY telemetryUpdated)
    Q_PROPERTY(double zramOrigMib READ zramOrigMib NOTIFY telemetryUpdated)
    Q_PROPERTY(double zramComprMib READ zramComprMib NOTIFY telemetryUpdated)
    Q_PROPERTY(double zramMemUsedMib READ zramMemUsedMib NOTIFY telemetryUpdated)
    Q_PROPERTY(double zramCompressionRatio READ zramCompressionRatio NOTIFY telemetryUpdated)
    Q_PROPERTY(double swapUsedGib READ swapUsedGib NOTIFY telemetryUpdated)
    Q_PROPERTY(double swapTotalGib READ swapTotalGib NOTIFY telemetryUpdated)
    Q_PROPERTY(bool swapAvailable READ swapAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool zramAvailable READ zramAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool zramCompressionAvailable READ zramCompressionAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool diskThroughputAvailable READ diskThroughputAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(double diskReadMibPerSec READ diskReadMibPerSec NOTIFY telemetryUpdated)
    Q_PROPERTY(double diskWriteMibPerSec READ diskWriteMibPerSec NOTIFY telemetryUpdated)

    // Power & Cooling properties
    Q_PROPERTY(bool onAcPower READ onAcPower NOTIFY telemetryUpdated)
    Q_PROPERTY(bool batteryPresent READ batteryPresent NOTIFY telemetryUpdated)
    Q_PROPERTY(bool batteryStateAvailable READ batteryStateAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool batteryLevelAvailable READ batteryLevelAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool batteryHealthAvailable READ batteryHealthAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool batteryPowerAvailable READ batteryPowerAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY telemetryUpdated)
    Q_PROPERTY(double batteryPercent READ batteryPercent NOTIFY telemetryUpdated)
    Q_PROPERTY(double batteryWatts READ batteryWatts NOTIFY telemetryUpdated)
    Q_PROPERTY(int batteryTimeRemainingMinutes READ batteryTimeRemainingMinutes NOTIFY telemetryUpdated)
    Q_PROPERTY(QString batteryTimeRemainingText READ batteryTimeRemainingText NOTIFY telemetryUpdated)
    Q_PROPERTY(double batteryHealthPercent READ batteryHealthPercent NOTIFY telemetryUpdated)
    Q_PROPERTY(bool raplExactAvailable READ raplExactAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool packagePowerAvailable READ packagePowerAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool packagePowerEstimated READ packagePowerEstimated NOTIFY telemetryUpdated)
    Q_PROPERTY(double packageWatts READ packageWatts NOTIFY telemetryUpdated)
    Q_PROPERTY(double pl1LimitWatts READ pl1LimitWatts NOTIFY telemetryUpdated)
    Q_PROPERTY(double pl2LimitWatts READ pl2LimitWatts NOTIFY telemetryUpdated)
    Q_PROPERTY(bool powerLimitsAvailable READ powerLimitsAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(QString activePowerProfile READ activePowerProfile NOTIFY telemetryUpdated)
    Q_PROPERTY(QStringList availablePowerProfiles READ availablePowerProfiles NOTIFY telemetryUpdated)
    Q_PROPERTY(bool powerProfileServiceAvailable READ powerProfileServiceAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(int primaryFanRpm READ primaryFanRpm NOTIFY telemetryUpdated)
    Q_PROPERTY(bool fanAvailable READ fanAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(double nvmeTempCelsius READ nvmeTempCelsius NOTIFY telemetryUpdated)
    Q_PROPERTY(double wifiTempCelsius READ wifiTempCelsius NOTIFY telemetryUpdated)
    Q_PROPERTY(bool nvmeTempAvailable READ nvmeTempAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool wifiTempAvailable READ wifiTempAvailable NOTIFY telemetryUpdated)

    // Causal Verdict & Culprit properties
    Q_PROPERTY(int verdictSeverity READ verdictSeverity NOTIFY telemetryUpdated)
    Q_PROPERTY(QString verdictHeadline READ verdictHeadline NOTIFY telemetryUpdated)
    Q_PROPERTY(QString verdictCausalChain READ verdictCausalChain NOTIFY telemetryUpdated)
    Q_PROPERTY(QString verdictRecommendation READ verdictRecommendation NOTIFY telemetryUpdated)
    Q_PROPERTY(QString primaryBottleneckDomain READ primaryBottleneckDomain NOTIFY telemetryUpdated)
    Q_PROPERTY(QString culpritName READ culpritName NOTIFY telemetryUpdated)
    Q_PROPERTY(int culpritPid READ culpritPid NOTIFY telemetryUpdated)
    Q_PROPERTY(double culpritCpuPercent READ culpritCpuPercent NOTIFY telemetryUpdated)
    Q_PROPERTY(double culpritRssMb READ culpritRssMb NOTIFY telemetryUpdated)
    Q_PROPERTY(QString memoryCulpritName READ memoryCulpritName NOTIFY telemetryUpdated)
    Q_PROPERTY(int memoryCulpritPid READ memoryCulpritPid NOTIFY telemetryUpdated)
    Q_PROPERTY(double memoryCulpritRssMb READ memoryCulpritRssMb NOTIFY telemetryUpdated)

    // Structured collections for QML views
    Q_PROPERTY(QVariantList domainSensors READ domainSensors NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList coreItems READ coreItems NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList topProcesses READ topProcesses NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList historySamples READ historySamples NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList incidentHistory READ incidentHistory NOTIFY telemetryUpdated)
    Q_PROPERTY(double historyMaxWatts READ historyMaxWatts NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantMap metricMetadata READ metricMetadata NOTIFY telemetryUpdated)
    Q_PROPERTY(int selectedHistoryIndex READ selectedHistoryIndex WRITE setSelectedHistoryIndex NOTIFY selectedHistoryChanged)
    Q_PROPERTY(qulonglong selectedHistorySampleId READ selectedHistorySampleId WRITE setSelectedHistorySampleId NOTIFY selectedHistoryChanged)
    Q_PROPERTY(bool selectedHistoryExpired READ selectedHistoryExpired NOTIFY selectedHistoryChanged)
    Q_PROPERTY(QVariantMap selectedHistoryDetail READ selectedHistoryDetail NOTIFY selectedHistoryChanged)

    Q_PROPERTY(int pollIntervalMs READ pollIntervalMs WRITE setPollIntervalMs NOTIFY pollIntervalChanged)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY notificationsEnabledChanged)
    Q_PROPERTY(bool recordingPaused READ recordingPaused WRITE setRecordingPaused NOTIFY recordingPausedChanged)
    Q_PROPERTY(QString lastActionStatus READ lastActionStatus NOTIFY lastActionStatusChanged)
    Q_PROPERTY(bool lastActionSucceeded READ lastActionSucceeded NOTIFY lastActionStatusChanged)
    Q_PROPERTY(QString lastActionPath READ lastActionPath NOTIFY lastActionStatusChanged)
    Q_PROPERTY(bool powerProfileChangePending READ powerProfileChangePending NOTIFY lastActionStatusChanged)

public:
    explicit PulseTelemetryController(QObject *parent = nullptr);
    explicit PulseTelemetryController(const QDBusConnection &powerProfileBus, QObject *parent = nullptr);

    double cpuLoadPercent() const { return m_cpu.totalLoadPercent; }
    double cpuPackageTemp() const { return m_cpu.packageTempCelsius; }
    double thermalHeadroom() const { return m_cpu.thermalHeadroomCelsius; }
    double thermalVelocity() const { return m_cpu.thermalVelocityCPerSec; }
    int cpuAvgFreqMhz() const { return m_cpu.avgFreqMhz; }
    int cpuMaxFreqMhz() const { return m_cpu.maxFreqMhz; }
    int cpuPeakFreqMhz() const { return m_cpu.peakCoreFreqMhz; }
    qint64 totalThrottleEvents() const { return m_cpu.totalThrottleEvents; }
    qint64 deltaThrottleEvents() const { return m_cpu.deltaThrottleEvents; }
    bool cpuLoadAvailable() const { return m_cpu.loadAvailable && m_cpu.loadWarmedUp; }
    bool cpuTempAvailable() const { return m_cpu.temperatureAvailable; }
    bool thermalHeadroomAvailable() const { return m_cpu.temperatureAvailable && m_cpu.thermalLimitAvailable; }
    bool throttleCountersAvailable() const { return m_cpu.throttleCountersAvailable; }

    bool gpuAvailable() const { return m_gpu.available; }
    QString gpuDriverName() const { return m_gpu.driverName; }
    int gpuCurrentFreqMhz() const { return m_gpu.currentFreqMhz; }
    int gpuBoostFreqMhz() const { return m_gpu.boostFreqMhz; }
    double gpuBusyPercent() const { return m_gpu.busyPercent; }
    double gpuTempCelsius() const { return m_gpu.tempCelsius; }
    double gpuVramUsedMib() const { return m_gpu.vramUsedMegabytes; }
    double gpuVramTotalMib() const { return m_gpu.vramTotalMegabytes; }
    double gpuPowerWatts() const { return m_gpu.powerWatts; }
    bool gpuFrequencyAvailable() const { return m_gpu.frequencyAvailable; }
    bool gpuBusyAvailable() const { return m_gpu.busyAvailable; }
    bool gpuTempAvailable() const { return m_gpu.temperatureAvailable; }
    bool gpuPowerAvailable() const { return m_gpu.powerAvailable; }
    bool gpuVramAvailable() const { return m_gpu.vramAvailable; }

    double psiCpuSome10() const { return m_psiMem.cpuPsi.someAvg10; }
    double psiMemSome10() const { return m_psiMem.memoryPsi.someAvg10; }
    double psiMemFull10() const { return m_psiMem.memoryPsi.fullAvg10; }
    double psiIoSome10() const { return m_psiMem.ioPsi.someAvg10; }
    double psiIoFull10() const { return m_psiMem.ioPsi.fullAvg10; }
    double psiCpuStallMs() const { return m_psiMem.cpuPsi.deltaStallMsPerSec; }
    double psiMemStallMs() const { return m_psiMem.memoryPsi.deltaStallMsPerSec; }
    double psiIoStallMs() const { return m_psiMem.ioPsi.deltaStallMsPerSec; }
    double responsivenessScore() const { return m_psiMem.responsivenessScore; }
    bool psiAvailable() const { return m_psiMem.responsivenessAvailable; }
    bool psiCpuAvailable() const { return m_psiMem.cpuPsi.someAvailable; }
    bool psiMemoryAvailable() const { return m_psiMem.memoryPsi.someAvailable; }
    bool psiMemoryFullAvailable() const { return m_psiMem.memoryPsi.fullAvailable; }
    bool psiIoAvailable() const { return m_psiMem.ioPsi.someAvailable; }
    bool psiIoFullAvailable() const { return m_psiMem.ioPsi.fullAvailable; }
    bool memoryAvailable() const { return m_psiMem.ramAvailable; }
    double ramUsedPercent() const { return m_psiMem.ramUsedPercent; }
    double ramUsedGib() const { return static_cast<double>(m_psiMem.ramUsedBytes) / (1024.0 * 1024.0 * 1024.0); }
    double ramTotalGib() const { return static_cast<double>(m_psiMem.ramTotalBytes) / (1024.0 * 1024.0 * 1024.0); }
    bool zramActive() const { return m_psiMem.zramActive; }
    double zramOrigMib() const { return static_cast<double>(m_psiMem.zramOrigBytes) / (1024.0 * 1024.0); }
    double zramComprMib() const { return static_cast<double>(m_psiMem.zramCompressedBytes) / (1024.0 * 1024.0); }
    double zramMemUsedMib() const { return static_cast<double>(m_psiMem.zramMemUsedTotalBytes) / (1024.0 * 1024.0); }
    double zramCompressionRatio() const { return m_psiMem.zramCompressionRatio; }
    double swapUsedGib() const
    {
        const quint64 used = (m_psiMem.swapTotalBytes > m_psiMem.swapFreeBytes)
            ? (m_psiMem.swapTotalBytes - m_psiMem.swapFreeBytes)
            : 0ULL;
        return static_cast<double>(used) / (1024.0 * 1024.0 * 1024.0);
    }
    double swapTotalGib() const { return static_cast<double>(m_psiMem.swapTotalBytes) / (1024.0 * 1024.0 * 1024.0); }
    bool swapAvailable() const { return m_psiMem.swapAvailable; }
    bool zramAvailable() const { return m_psiMem.zramAvailable; }
    bool zramCompressionAvailable() const { return m_psiMem.zramCompressionAvailable; }
    bool diskThroughputAvailable() const { return m_psiMem.diskThroughputAvailable; }
    double diskReadMibPerSec() const { return m_psiMem.diskReadMibPerSec; }
    double diskWriteMibPerSec() const { return m_psiMem.diskWriteMibPerSec; }

    bool onAcPower() const { return m_power.onAcPower; }
    bool batteryPresent() const { return m_power.batteryPresent; }
    bool batteryStateAvailable() const { return m_power.batteryStateAvailable; }
    bool batteryLevelAvailable() const { return m_power.batteryLevelAvailable; }
    bool batteryHealthAvailable() const { return m_power.batteryHealthAvailable; }
    bool batteryPowerAvailable() const { return m_power.batteryPowerAvailable; }
    QString batteryStatus() const { return m_power.batteryStatus; }
    double batteryPercent() const { return m_power.batteryPercent; }
    double batteryWatts() const { return m_power.batteryWatts; }
    int batteryTimeRemainingMinutes() const { return m_power.batteryTimeRemainingMinutes; }
    QString batteryTimeRemainingText() const;
    double batteryHealthPercent() const { return m_power.batteryHealthPercent; }
    bool raplExactAvailable() const { return m_power.raplExactAvailable; }
    bool packagePowerAvailable() const { return m_power.packagePowerAvailable; }
    bool packagePowerEstimated() const { return m_power.packagePowerEstimated; }
    double packageWatts() const { return m_power.packageWatts; }
    double pl1LimitWatts() const { return m_power.pl1LimitWatts; }
    double pl2LimitWatts() const { return m_power.pl2LimitWatts; }
    bool powerLimitsAvailable() const { return m_power.pl1LimitWatts > 0.0 || m_power.pl2LimitWatts > 0.0; }
    QString activePowerProfile() const { return m_activePowerProfile; }
    QStringList availablePowerProfiles() const { return m_availablePowerProfiles; }
    bool powerProfileServiceAvailable() const { return m_powerProfileServiceAvailable; }
    int primaryFanRpm() const { return m_power.primaryFanRpm; }
    bool fanAvailable() const { return m_power.fanAvailable; }
    double nvmeTempCelsius() const { return m_nvmeTemp; }
    double wifiTempCelsius() const { return m_wifiTemp; }
    bool nvmeTempAvailable() const { return m_nvmeTempAvailable; }
    bool wifiTempAvailable() const { return m_wifiTempAvailable; }

    int verdictSeverity() const { return static_cast<int>(m_verdict.severity); }
    QString verdictHeadline() const { return m_verdict.headline; }
    QString verdictCausalChain() const { return m_verdict.causalChain; }
    QString verdictRecommendation() const { return m_verdict.recommendation; }
    QString primaryBottleneckDomain() const { return m_verdict.primaryBottleneckDomain; }
    QString culpritName() const { return m_verdict.topProcess.name; }
    int culpritPid() const { return m_verdict.topProcess.pid; }
    double culpritCpuPercent() const { return m_verdict.topProcess.cpuPercent; }
    double culpritRssMb() const { return m_verdict.topProcess.rssMegabytes; }
    QString memoryCulpritName() const { return m_verdict.topMemoryProcess.name; }
    int memoryCulpritPid() const { return m_verdict.topMemoryProcess.pid; }
    double memoryCulpritRssMb() const { return m_verdict.topMemoryProcess.rssMegabytes; }

    QVariantList domainSensors() const { return m_domainSensorsVariant; }
    QVariantList coreItems() const { return m_coreItemsVariant; }
    QVariantList topProcesses() const { return m_topProcessesVariant; }
    QVariantList historySamples() const { return m_historySamplesVariant; }
    QVariantList incidentHistory() const { return m_incidentHistoryVariant; }
    double historyMaxWatts() const { return m_historyMaxWatts; }
    QVariantMap metricMetadata() const;

    int selectedHistoryIndex() const { return m_selectedHistorySampleId == 0 ? -1 : m_recorder.indexOfSample(m_selectedHistorySampleId); }
    void setSelectedHistoryIndex(int index);
    qulonglong selectedHistorySampleId() const { return m_selectedHistorySampleId; }
    void setSelectedHistorySampleId(qulonglong sampleId);
    bool selectedHistoryExpired() const { return m_selectedHistorySampleId != 0 && m_recorder.indexOfSample(m_selectedHistorySampleId) < 0; }
    QVariantMap selectedHistoryDetail() const;

    int pollIntervalMs() const { return m_timer.interval(); }
    void setPollIntervalMs(int intervalMs);

    bool notificationsEnabled() const { return m_notificationsEnabled; }
    void setNotificationsEnabled(bool enabled);

    bool recordingPaused() const { return m_recordingPaused; }
    void setRecordingPaused(bool paused);

    QString lastActionStatus() const { return m_lastActionStatus; }
    bool lastActionSucceeded() const { return m_lastActionSucceeded; }
    QString lastActionPath() const { return m_lastActionPath; }
    bool powerProfileChangePending() const { return m_powerProfileChangePending; }

    Q_INVOKABLE void refreshNow();
    Q_INVOKABLE bool setPowerProfile(const QString &profile);
    Q_INVOKABLE void launchStudioApp(const QString &pageName = QString());
    Q_INVOKABLE bool openSystemMonitor();
    Q_INVOKABLE bool openReportsFolder();
    Q_INVOKABLE int jumpToPeakThermalSpike();
    Q_INVOKABLE int jumpToWorstStallSpike();
    Q_INVOKABLE QString copyIncidentReportToClipboard();
    Q_INVOKABLE QString exportIncidentReportToFile();

Q_SIGNALS:
    void telemetryUpdated();
    void selectedHistoryChanged();
    void pollIntervalChanged();
    void notificationsEnabledChanged();
    void recordingPausedChanged();
    void lastActionStatusChanged();

private:
    void maybeEmitAlertNotifications();
    void setLastActionStatus(const QString &status);
    void setActionResult(bool succeeded, const QString &status, const QString &path = QString());
    void refreshPowerProfileProperties();
    static QVariantMap sampleToVariantMap(const FlightSample &s, int index);

    SysfsReader m_sysfsReader;
    PsiMemorySampler m_psiSampler;
    CpuSocSampler m_cpuSampler;
    PowerFlowSampler m_powerSampler;
    FlightRecorder m_recorder;
    QDBusConnection m_powerProfileBus;

    QTimer m_timer;
    QTimer m_powerProfileRefreshTimer;
    QElapsedTimer m_uptime;
    qint64 m_lastBoottimeMs = 0;
    qint64 m_lastSampleWallTimeMs = 0;
    qint64 m_lastProcessScanBoottimeMs = 0;
    int m_processScanWarmupCount = 0;
    bool m_gapPending = false;
    bool m_notificationsEnabled = true;
    bool m_recordingPaused = false;
    qint64 m_lastAlertTimestampMs = 0;
    QString m_lastActionStatus;
    bool m_lastActionSucceeded = false;
    QString m_lastActionPath;
    QString m_reportsDirectory;
    quint64 m_selectedHistorySampleId = 0;
    QVariantMap m_selectedHistorySnapshot;
    VerdictStabilizer m_verdictStabilizer;
    QString m_activePowerProfile;
    QStringList m_availablePowerProfiles;
    bool m_powerProfileServiceAvailable = false;
    bool m_powerProfileChangePending = false;

    QList<DomainSensorReading> m_domains;
    CpuSocSnapshot m_cpu;
    GpuSnapshot m_gpu;
    PsiAndMemorySnapshot m_psiMem;
    PowerFlowSnapshot m_power;
    ProcessRadarSnapshot m_processRadar;
    CausalVerdict m_verdict;
    double m_nvmeTemp = 0.0;
    double m_wifiTemp = 0.0;
    bool m_nvmeTempAvailable = false;
    bool m_wifiTempAvailable = false;

    QVariantList m_domainSensorsVariant;
    QVariantList m_coreItemsVariant;
    QVariantList m_topProcessesVariant;
    QVariantList m_historySamplesVariant;
    QVariantList m_incidentHistoryVariant;
    double m_historyMaxWatts = 65.0;
    int m_selectedHistoryIndex = -1;
};

} // namespace LoofiPulse
