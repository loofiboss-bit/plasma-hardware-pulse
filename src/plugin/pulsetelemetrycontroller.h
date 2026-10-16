#pragma once

#include "causaldiagnosisengine.h"
#include "cpusocsampler.h"
#include "flightrecorder.h"
#include "powerflowsampler.h"
#include "psimemorysampler.h"
#include "sysfsreader.h"
#include "telemetrytypes.h"

#include <QElapsedTimer>
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

    // Power & Cooling properties
    Q_PROPERTY(bool onAcPower READ onAcPower NOTIFY telemetryUpdated)
    Q_PROPERTY(bool batteryPresent READ batteryPresent NOTIFY telemetryUpdated)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY telemetryUpdated)
    Q_PROPERTY(double batteryPercent READ batteryPercent NOTIFY telemetryUpdated)
    Q_PROPERTY(double batteryWatts READ batteryWatts NOTIFY telemetryUpdated)
    Q_PROPERTY(int batteryTimeRemainingMinutes READ batteryTimeRemainingMinutes NOTIFY telemetryUpdated)
    Q_PROPERTY(QString batteryTimeRemainingText READ batteryTimeRemainingText NOTIFY telemetryUpdated)
    Q_PROPERTY(double batteryHealthPercent READ batteryHealthPercent NOTIFY telemetryUpdated)
    Q_PROPERTY(bool raplExactAvailable READ raplExactAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(double packageWatts READ packageWatts NOTIFY telemetryUpdated)
    Q_PROPERTY(double pl1LimitWatts READ pl1LimitWatts NOTIFY telemetryUpdated)
    Q_PROPERTY(double pl2LimitWatts READ pl2LimitWatts NOTIFY telemetryUpdated)
    Q_PROPERTY(QString activePowerProfile READ activePowerProfile NOTIFY telemetryUpdated)
    Q_PROPERTY(int primaryFanRpm READ primaryFanRpm NOTIFY telemetryUpdated)
    Q_PROPERTY(double nvmeTempCelsius READ nvmeTempCelsius NOTIFY telemetryUpdated)
    Q_PROPERTY(double wifiTempCelsius READ wifiTempCelsius NOTIFY telemetryUpdated)

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
    Q_PROPERTY(int selectedHistoryIndex READ selectedHistoryIndex WRITE setSelectedHistoryIndex NOTIFY selectedHistoryChanged)
    Q_PROPERTY(QVariantMap selectedHistoryDetail READ selectedHistoryDetail NOTIFY selectedHistoryChanged)

    Q_PROPERTY(int pollIntervalMs READ pollIntervalMs WRITE setPollIntervalMs NOTIFY pollIntervalChanged)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY notificationsEnabledChanged)
    Q_PROPERTY(bool recordingPaused READ recordingPaused WRITE setRecordingPaused NOTIFY recordingPausedChanged)
    Q_PROPERTY(QString lastActionStatus READ lastActionStatus NOTIFY lastActionStatusChanged)

public:
    explicit PulseTelemetryController(QObject *parent = nullptr);

    double cpuLoadPercent() const { return m_cpu.totalLoadPercent; }
    double cpuPackageTemp() const { return m_cpu.packageTempCelsius; }
    double thermalHeadroom() const { return m_cpu.thermalHeadroomCelsius; }
    double thermalVelocity() const { return m_cpu.thermalVelocityCPerSec; }
    int cpuAvgFreqMhz() const { return m_cpu.avgFreqMhz; }
    int cpuMaxFreqMhz() const { return m_cpu.maxFreqMhz; }
    int cpuPeakFreqMhz() const { return m_cpu.peakCoreFreqMhz; }
    qint64 totalThrottleEvents() const { return m_cpu.totalThrottleEvents; }
    qint64 deltaThrottleEvents() const { return m_cpu.deltaThrottleEvents; }

    bool gpuAvailable() const { return m_gpu.available; }
    QString gpuDriverName() const { return m_gpu.driverName; }
    int gpuCurrentFreqMhz() const { return m_gpu.currentFreqMhz; }
    int gpuBoostFreqMhz() const { return m_gpu.boostFreqMhz; }
    double gpuBusyPercent() const { return m_gpu.busyPercent; }
    double gpuTempCelsius() const { return m_gpu.tempCelsius; }
    double gpuVramUsedMib() const { return m_gpu.vramUsedMegabytes; }
    double gpuVramTotalMib() const { return m_gpu.vramTotalMegabytes; }
    double gpuPowerWatts() const { return m_gpu.powerWatts; }

    double psiCpuSome10() const { return m_psiMem.cpuPsi.someAvg10; }
    double psiMemSome10() const { return m_psiMem.memoryPsi.someAvg10; }
    double psiMemFull10() const { return m_psiMem.memoryPsi.fullAvg10; }
    double psiIoSome10() const { return m_psiMem.ioPsi.someAvg10; }
    double psiIoFull10() const { return m_psiMem.ioPsi.fullAvg10; }
    double psiCpuStallMs() const { return m_psiMem.cpuPsi.deltaStallMsPerSec; }
    double psiMemStallMs() const { return m_psiMem.memoryPsi.deltaStallMsPerSec; }
    double psiIoStallMs() const { return m_psiMem.ioPsi.deltaStallMsPerSec; }
    double responsivenessScore() const { return m_psiMem.responsivenessScore; }
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

    bool onAcPower() const { return m_power.onAcPower; }
    bool batteryPresent() const { return m_power.batteryPresent; }
    QString batteryStatus() const { return m_power.batteryStatus; }
    double batteryPercent() const { return m_power.batteryPercent; }
    double batteryWatts() const { return m_power.batteryWatts; }
    int batteryTimeRemainingMinutes() const { return m_power.batteryTimeRemainingMinutes; }
    QString batteryTimeRemainingText() const;
    double batteryHealthPercent() const { return m_power.batteryHealthPercent; }
    bool raplExactAvailable() const { return m_power.raplExactAvailable; }
    double packageWatts() const { return m_power.packageWatts; }
    double pl1LimitWatts() const { return m_power.pl1LimitWatts; }
    double pl2LimitWatts() const { return m_power.pl2LimitWatts; }
    QString activePowerProfile() const { return m_power.activePowerProfile; }
    int primaryFanRpm() const { return m_power.primaryFanRpm; }
    double nvmeTempCelsius() const { return m_nvmeTemp; }
    double wifiTempCelsius() const { return m_wifiTemp; }

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

    int selectedHistoryIndex() const { return m_selectedHistoryIndex; }
    void setSelectedHistoryIndex(int index);
    QVariantMap selectedHistoryDetail() const;

    int pollIntervalMs() const { return m_timer.interval(); }
    void setPollIntervalMs(int intervalMs);

    bool notificationsEnabled() const { return m_notificationsEnabled; }
    void setNotificationsEnabled(bool enabled);

    bool recordingPaused() const { return m_recordingPaused; }
    void setRecordingPaused(bool paused);

    QString lastActionStatus() const { return m_lastActionStatus; }

    Q_INVOKABLE void refreshNow();
    Q_INVOKABLE bool setPowerProfile(const QString &profile);
    Q_INVOKABLE void launchStudioApp();
    Q_INVOKABLE int jumpToPeakThermalSpike();
    Q_INVOKABLE int jumpToWorstStallSpike();
    Q_INVOKABLE bool terminateProcess(int pid, const QString &name = QString());
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
    static QVariantMap sampleToVariantMap(const FlightSample &s, int index);

    SysfsReader m_sysfsReader;
    PsiMemorySampler m_psiSampler;
    CpuSocSampler m_cpuSampler;
    PowerFlowSampler m_powerSampler;
    FlightRecorder m_recorder;

    QTimer m_timer;
    QElapsedTimer m_elapsed;
    bool m_notificationsEnabled = true;
    bool m_recordingPaused = false;
    qint64 m_lastAlertTimestampMs = 0;
    QString m_lastActionStatus;

    QList<DomainSensorReading> m_domains;
    CpuSocSnapshot m_cpu;
    GpuSnapshot m_gpu;
    PsiAndMemorySnapshot m_psiMem;
    PowerFlowSnapshot m_power;
    ProcessRadarSnapshot m_processRadar;
    CausalVerdict m_verdict;
    double m_nvmeTemp = 0.0;
    double m_wifiTemp = 0.0;

    QVariantList m_domainSensorsVariant;
    QVariantList m_coreItemsVariant;
    QVariantList m_topProcessesVariant;
    QVariantList m_historySamplesVariant;
    int m_selectedHistoryIndex = -1;
};

} // namespace LoofiPulse
