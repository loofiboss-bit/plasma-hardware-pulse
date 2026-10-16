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

    // PSI & ZRAM / Memory properties
    Q_PROPERTY(double psiCpuSome10 READ psiCpuSome10 NOTIFY telemetryUpdated)
    Q_PROPERTY(double psiMemSome10 READ psiMemSome10 NOTIFY telemetryUpdated)
    Q_PROPERTY(double psiMemFull10 READ psiMemFull10 NOTIFY telemetryUpdated)
    Q_PROPERTY(double psiIoSome10 READ psiIoSome10 NOTIFY telemetryUpdated)
    Q_PROPERTY(double psiIoFull10 READ psiIoFull10 NOTIFY telemetryUpdated)
    Q_PROPERTY(double responsivenessScore READ responsivenessScore NOTIFY telemetryUpdated)
    Q_PROPERTY(double ramUsedPercent READ ramUsedPercent NOTIFY telemetryUpdated)
    Q_PROPERTY(double ramUsedGib READ ramUsedGib NOTIFY telemetryUpdated)
    Q_PROPERTY(double ramTotalGib READ ramTotalGib NOTIFY telemetryUpdated)
    Q_PROPERTY(bool zramActive READ zramActive NOTIFY telemetryUpdated)
    Q_PROPERTY(double zramOrigMib READ zramOrigMib NOTIFY telemetryUpdated)
    Q_PROPERTY(double zramComprMib READ zramComprMib NOTIFY telemetryUpdated)
    Q_PROPERTY(double zramCompressionRatio READ zramCompressionRatio NOTIFY telemetryUpdated)

    // Power & Cooling properties
    Q_PROPERTY(bool onAcPower READ onAcPower NOTIFY telemetryUpdated)
    Q_PROPERTY(bool batteryPresent READ batteryPresent NOTIFY telemetryUpdated)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY telemetryUpdated)
    Q_PROPERTY(double batteryPercent READ batteryPercent NOTIFY telemetryUpdated)
    Q_PROPERTY(double batteryWatts READ batteryWatts NOTIFY telemetryUpdated)
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

    // Structured collections for QML views
    Q_PROPERTY(QVariantList domainSensors READ domainSensors NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList coreItems READ coreItems NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList historySamples READ historySamples NOTIFY telemetryUpdated)
    Q_PROPERTY(int selectedHistoryIndex READ selectedHistoryIndex WRITE setSelectedHistoryIndex NOTIFY selectedHistoryChanged)
    Q_PROPERTY(QVariantMap selectedHistoryDetail READ selectedHistoryDetail NOTIFY selectedHistoryChanged)

    Q_PROPERTY(int pollIntervalMs READ pollIntervalMs WRITE setPollIntervalMs NOTIFY pollIntervalChanged)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY notificationsEnabledChanged)

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

    double psiCpuSome10() const { return m_psiMem.cpuPsi.someAvg10; }
    double psiMemSome10() const { return m_psiMem.memoryPsi.someAvg10; }
    double psiMemFull10() const { return m_psiMem.memoryPsi.fullAvg10; }
    double psiIoSome10() const { return m_psiMem.ioPsi.someAvg10; }
    double psiIoFull10() const { return m_psiMem.ioPsi.fullAvg10; }
    double responsivenessScore() const { return m_psiMem.responsivenessScore; }
    double ramUsedPercent() const { return m_psiMem.ramUsedPercent; }
    double ramUsedGib() const { return static_cast<double>(m_psiMem.ramUsedBytes) / (1024.0 * 1024.0 * 1024.0); }
    double ramTotalGib() const { return static_cast<double>(m_psiMem.ramTotalBytes) / (1024.0 * 1024.0 * 1024.0); }
    bool zramActive() const { return m_psiMem.zramActive; }
    double zramOrigMib() const { return static_cast<double>(m_psiMem.zramOrigBytes) / (1024.0 * 1024.0); }
    double zramComprMib() const { return static_cast<double>(m_psiMem.zramCompressedBytes) / (1024.0 * 1024.0); }
    double zramCompressionRatio() const { return m_psiMem.zramCompressionRatio; }

    bool onAcPower() const { return m_power.onAcPower; }
    bool batteryPresent() const { return m_power.batteryPresent; }
    QString batteryStatus() const { return m_power.batteryStatus; }
    double batteryPercent() const { return m_power.batteryPercent; }
    double batteryWatts() const { return m_power.batteryWatts; }
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

    QVariantList domainSensors() const { return m_domainSensorsVariant; }
    QVariantList coreItems() const { return m_coreItemsVariant; }
    QVariantList historySamples() const { return m_historySamplesVariant; }

    int selectedHistoryIndex() const { return m_selectedHistoryIndex; }
    void setSelectedHistoryIndex(int index);
    QVariantMap selectedHistoryDetail() const;

    int pollIntervalMs() const { return m_timer.interval(); }
    void setPollIntervalMs(int intervalMs);

    bool notificationsEnabled() const { return m_notificationsEnabled; }
    void setNotificationsEnabled(bool enabled);

    Q_INVOKABLE void refreshNow();
    Q_INVOKABLE bool setPowerProfile(const QString &profile);
    Q_INVOKABLE void launchStudioApp();
    Q_INVOKABLE int jumpToPeakThermalSpike();
    Q_INVOKABLE int jumpToWorstStallSpike();

Q_SIGNALS:
    void telemetryUpdated();
    void selectedHistoryChanged();
    void pollIntervalChanged();
    void notificationsEnabledChanged();

private:
    void maybeEmitAlertNotifications();
    static QVariantMap sampleToVariantMap(const FlightSample &s, int index);

    SysfsReader m_sysfsReader;
    PsiMemorySampler m_psiSampler;
    CpuSocSampler m_cpuSampler;
    PowerFlowSampler m_powerSampler;
    FlightRecorder m_recorder;

    QTimer m_timer;
    QElapsedTimer m_elapsed;
    bool m_notificationsEnabled = true;
    qint64 m_lastAlertTimestampMs = 0;

    QList<DomainSensorReading> m_domains;
    CpuSocSnapshot m_cpu;
    GpuSnapshot m_gpu;
    PsiAndMemorySnapshot m_psiMem;
    PowerFlowSnapshot m_power;
    CausalVerdict m_verdict;
    double m_nvmeTemp = 0.0;
    double m_wifiTemp = 0.0;

    QVariantList m_domainSensorsVariant;
    QVariantList m_coreItemsVariant;
    QVariantList m_historySamplesVariant;
    int m_selectedHistoryIndex = -1;
};

} // namespace LoofiPulse
