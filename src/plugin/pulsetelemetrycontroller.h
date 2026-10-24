#pragma once

#include "causaldiagnosisengine.h"
#include "cpusocsampler.h"
#include "flightrecorder.h"
#include "powerflowsampler.h"
#include "psimemorysampler.h"
#include "sysfsreader.h"
#include "telemetrytypes.h"

#include <QElapsedTimer>
#include <QDBusServiceWatcher>
#include <QThreadPool>
#include <QUuid>
#include <functional>
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
    Q_PROPERTY(bool netThroughputAvailable READ netThroughputAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(double netRxMibPerSec READ netRxMibPerSec NOTIFY telemetryUpdated)
    Q_PROPERTY(double netTxMibPerSec READ netTxMibPerSec NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList networkInterfaces READ networkInterfaces NOTIFY telemetryUpdated)
    Q_PROPERTY(QStringList selectedNetworkInterfaces READ selectedNetworkInterfaces WRITE setSelectedNetworkInterfaces NOTIFY selectedNetworkInterfacesChanged)

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
    Q_PROPERTY(QVariantList powerProfileHolds READ powerProfileHolds NOTIFY telemetryUpdated)
    Q_PROPERTY(QString powerProfileHoldsSummary READ powerProfileHoldsSummary NOTIFY telemetryUpdated)
    Q_PROPERTY(bool powerProfileServiceAvailable READ powerProfileServiceAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(int primaryFanRpm READ primaryFanRpm NOTIFY telemetryUpdated)
    Q_PROPERTY(bool fanAvailable READ fanAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList fans READ fans NOTIFY telemetryUpdated)
    Q_PROPERTY(double nvmeTempCelsius READ nvmeTempCelsius NOTIFY telemetryUpdated)
    Q_PROPERTY(double wifiTempCelsius READ wifiTempCelsius NOTIFY telemetryUpdated)
    Q_PROPERTY(bool nvmeTempAvailable READ nvmeTempAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(bool wifiTempAvailable READ wifiTempAvailable NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList nvmeControllers READ nvmeControllers NOTIFY telemetryUpdated)
    Q_PROPERTY(bool nvmeWarningActive READ nvmeWarningActive NOTIFY telemetryUpdated)

    // Causal Verdict & Culprit properties
    Q_PROPERTY(int verdictSeverity READ verdictSeverity NOTIFY telemetryUpdated)
    Q_PROPERTY(QString verdictHeadline READ verdictHeadline NOTIFY telemetryUpdated)
    Q_PROPERTY(QString verdictCausalChain READ verdictCausalChain NOTIFY telemetryUpdated)
    Q_PROPERTY(QString verdictRecommendation READ verdictRecommendation NOTIFY telemetryUpdated)
    Q_PROPERTY(QString verdictRuleId READ verdictRuleId NOTIFY telemetryUpdated)
    Q_PROPERTY(double verdictDurationSeconds READ verdictDurationSeconds NOTIFY telemetryUpdated)
    Q_PROPERTY(QStringList verdictEvidence READ verdictEvidence NOTIFY telemetryUpdated)
    Q_PROPERTY(QStringList verdictMissingEvidence READ verdictMissingEvidence NOTIFY telemetryUpdated)
    Q_PROPERTY(QString primaryBottleneckDomain READ primaryBottleneckDomain NOTIFY telemetryUpdated)
    Q_PROPERTY(QStringList verdictContributingSignals READ verdictContributingSignals NOTIFY telemetryUpdated)
    Q_PROPERTY(QString culpritName READ culpritName NOTIFY telemetryUpdated)
    Q_PROPERTY(int culpritPid READ culpritPid NOTIFY telemetryUpdated)
    Q_PROPERTY(double culpritCpuPercent READ culpritCpuPercent NOTIFY telemetryUpdated)
    Q_PROPERTY(double culpritRssMb READ culpritRssMb NOTIFY telemetryUpdated)
    Q_PROPERTY(QString memoryCulpritName READ memoryCulpritName NOTIFY telemetryUpdated)
    Q_PROPERTY(int memoryCulpritPid READ memoryCulpritPid NOTIFY telemetryUpdated)
    Q_PROPERTY(double memoryCulpritRssMb READ memoryCulpritRssMb NOTIFY telemetryUpdated)
    Q_PROPERTY(int diskWaitProcessCount READ diskWaitProcessCount NOTIFY telemetryUpdated)

    // Structured collections for QML views
    Q_PROPERTY(QVariantList domainSensors READ domainSensors NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList coreItems READ coreItems NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList topProcesses READ topProcesses NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList historySamples READ historySamples NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantMap historySummary READ historySummary NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList incidentHistory READ incidentHistory NOTIFY telemetryUpdated)
    Q_PROPERTY(double historyMaxWatts READ historyMaxWatts NOTIFY telemetryUpdated)
    Q_PROPERTY(double historyMaxDiskMibPerSec READ historyMaxDiskMibPerSec NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantMap metricMetadata READ metricMetadata NOTIFY telemetryUpdated)
    Q_PROPERTY(int selectedHistoryIndex READ selectedHistoryIndex WRITE setSelectedHistoryIndex NOTIFY selectedHistoryChanged)
    Q_PROPERTY(qulonglong selectedHistorySampleId READ selectedHistorySampleId WRITE setSelectedHistorySampleId NOTIFY selectedHistoryChanged)
    Q_PROPERTY(bool selectedHistoryExpired READ selectedHistoryExpired NOTIFY selectedHistoryChanged)
    Q_PROPERTY(QVariantMap selectedHistoryDetail READ selectedHistoryDetail NOTIFY selectedHistoryChanged)

    Q_PROPERTY(int pollIntervalMs READ pollIntervalMs WRITE setPollIntervalMs NOTIFY pollIntervalChanged)
    Q_PROPERTY(int historyRetentionSeconds READ historyRetentionSeconds WRITE setHistoryRetentionSeconds NOTIFY historyRetentionChanged)
    Q_PROPERTY(double actualHistorySeconds READ actualHistorySeconds NOTIFY telemetryUpdated)
    Q_PROPERTY(bool exportIncludeProcessDetails READ exportIncludeProcessDetails WRITE setExportIncludeProcessDetails NOTIFY exportIncludeProcessDetailsChanged)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY notificationsEnabledChanged)
    Q_PROPERTY(bool recordingPaused READ recordingPaused WRITE setRecordingPaused NOTIFY recordingPausedChanged)
    Q_PROPERTY(QString lastActionStatus READ lastActionStatus NOTIFY lastActionStatusChanged)
    Q_PROPERTY(bool lastActionSucceeded READ lastActionSucceeded NOTIFY lastActionStatusChanged)
    Q_PROPERTY(QString lastActionPath READ lastActionPath NOTIFY lastActionStatusChanged)
    Q_PROPERTY(bool powerProfileChangePending READ powerProfileChangePending NOTIFY lastActionStatusChanged)

public:
    ~PulseTelemetryController() override;
    QByteArray collectorPacket(bool includeHistory = false) const;
    QByteArray collectorHistory(qulonglong afterId, int limit = 64) const;
    QByteArray collectorIncident(const QString &id) const;
    QByteArray collectorMarkProblem();
    void setCollectorIdentity(const QString &id) { m_captureId = id; }
    explicit PulseTelemetryController(QObject *parent = nullptr);
    explicit PulseTelemetryController(const QDBusConnection &powerProfileBus, QObject *parent = nullptr);

    double cpuLoadPercent() const { return remoteValue<double>("cpuLoadPercent", m_cpu.totalLoadPercent); }
    double cpuPackageTemp() const { return remoteValue<double>("cpuPackageTemp", m_cpu.packageTempCelsius); }
    double thermalHeadroom() const { return remoteValue<double>("thermalHeadroom", m_cpu.thermalHeadroomCelsius); }
    double thermalVelocity() const { return remoteValue<double>("thermalVelocity", m_cpu.thermalVelocityCPerSec); }
    int cpuAvgFreqMhz() const { return remoteValue<int>("cpuAvgFreqMhz", m_cpu.avgFreqMhz); }
    int cpuMaxFreqMhz() const { return remoteValue<int>("cpuMaxFreqMhz", m_cpu.maxFreqMhz); }
    int cpuPeakFreqMhz() const { return remoteValue<int>("cpuPeakFreqMhz", m_cpu.peakCoreFreqMhz); }
    qint64 totalThrottleEvents() const { return remoteValue<qint64>("totalThrottleEvents", m_cpu.totalThrottleEvents); }
    qint64 deltaThrottleEvents() const { return remoteValue<qint64>("deltaThrottleEvents", m_cpu.deltaThrottleEvents); }
    bool cpuLoadAvailable() const { return remoteValue<bool>("cpuLoadAvailable", m_cpu.loadAvailable && m_cpu.loadWarmedUp); }
    bool cpuTempAvailable() const { return remoteValue<bool>("cpuTempAvailable", m_cpu.temperatureAvailable); }
    bool thermalHeadroomAvailable() const { return remoteValue<bool>("thermalHeadroomAvailable", m_cpu.temperatureAvailable && m_cpu.thermalLimitAvailable); }
    bool throttleCountersAvailable() const { return remoteValue<bool>("throttleCountersAvailable", m_cpu.throttleCountersAvailable); }

    bool gpuAvailable() const { return remoteValue<bool>("gpuAvailable", m_gpu.available); }
    QString gpuDriverName() const { return remoteValue<QString>("gpuDriverName", m_gpu.driverName); }
    int gpuCurrentFreqMhz() const { return remoteValue<int>("gpuCurrentFreqMhz", m_gpu.currentFreqMhz); }
    int gpuBoostFreqMhz() const { return remoteValue<int>("gpuBoostFreqMhz", m_gpu.boostFreqMhz); }
    double gpuBusyPercent() const { return remoteValue<double>("gpuBusyPercent", m_gpu.busyPercent); }
    double gpuTempCelsius() const { return remoteValue<double>("gpuTempCelsius", m_gpu.tempCelsius); }
    double gpuVramUsedMib() const { return remoteValue<double>("gpuVramUsedMib", m_gpu.vramUsedMegabytes); }
    double gpuVramTotalMib() const { return remoteValue<double>("gpuVramTotalMib", m_gpu.vramTotalMegabytes); }
    double gpuPowerWatts() const { return remoteValue<double>("gpuPowerWatts", m_gpu.powerWatts); }
    bool gpuFrequencyAvailable() const { return remoteValue<bool>("gpuFrequencyAvailable", m_gpu.frequencyAvailable); }
    bool gpuBusyAvailable() const { return remoteValue<bool>("gpuBusyAvailable", m_gpu.busyAvailable); }
    bool gpuTempAvailable() const { return remoteValue<bool>("gpuTempAvailable", m_gpu.temperatureAvailable); }
    bool gpuPowerAvailable() const { return remoteValue<bool>("gpuPowerAvailable", m_gpu.powerAvailable); }
    bool gpuVramAvailable() const { return remoteValue<bool>("gpuVramAvailable", m_gpu.vramAvailable); }

    double psiCpuSome10() const { return remoteValue<double>("psiCpuSome10", m_psiMem.cpuPsi.someAvg10); }
    double psiMemSome10() const { return remoteValue<double>("psiMemSome10", m_psiMem.memoryPsi.someAvg10); }
    double psiMemFull10() const { return remoteValue<double>("psiMemFull10", m_psiMem.memoryPsi.fullAvg10); }
    double psiIoSome10() const { return remoteValue<double>("psiIoSome10", m_psiMem.ioPsi.someAvg10); }
    double psiIoFull10() const { return remoteValue<double>("psiIoFull10", m_psiMem.ioPsi.fullAvg10); }
    double psiCpuStallMs() const { return remoteValue<double>("psiCpuStallMs", m_psiMem.cpuPsi.deltaStallMsPerSec); }
    double psiMemStallMs() const { return remoteValue<double>("psiMemStallMs", m_psiMem.memoryPsi.deltaStallMsPerSec); }
    double psiIoStallMs() const { return remoteValue<double>("psiIoStallMs", m_psiMem.ioPsi.deltaStallMsPerSec); }
    double responsivenessScore() const { return remoteValue<double>("responsivenessScore", m_psiMem.responsivenessScore); }
    bool psiAvailable() const { return remoteValue<bool>("psiAvailable", m_psiMem.responsivenessAvailable); }
    bool psiCpuAvailable() const { return remoteValue<bool>("psiCpuAvailable", m_psiMem.cpuPsi.someAvailable); }
    bool psiMemoryAvailable() const { return remoteValue<bool>("psiMemoryAvailable", m_psiMem.memoryPsi.someAvailable); }
    bool psiMemoryFullAvailable() const { return remoteValue<bool>("psiMemoryFullAvailable", m_psiMem.memoryPsi.fullAvailable); }
    bool psiIoAvailable() const { return remoteValue<bool>("psiIoAvailable", m_psiMem.ioPsi.someAvailable); }
    bool psiIoFullAvailable() const { return remoteValue<bool>("psiIoFullAvailable", m_psiMem.ioPsi.fullAvailable); }
    bool memoryAvailable() const { return remoteValue<bool>("memoryAvailable", m_psiMem.ramAvailable); }
    double ramUsedPercent() const { return remoteValue<double>("ramUsedPercent", m_psiMem.ramUsedPercent); }
    double ramUsedGib() const { return remoteValue<double>("ramUsedGib", static_cast<double>(m_psiMem.ramUsedBytes) / (1024.0 * 1024.0 * 1024.0)); }
    double ramTotalGib() const { return remoteValue<double>("ramTotalGib", static_cast<double>(m_psiMem.ramTotalBytes) / (1024.0 * 1024.0 * 1024.0)); }
    bool zramActive() const { return remoteValue<bool>("zramActive", m_psiMem.zramActive); }
    double zramOrigMib() const { return remoteValue<double>("zramOrigMib", static_cast<double>(m_psiMem.zramOrigBytes) / (1024.0 * 1024.0)); }
    double zramComprMib() const { return remoteValue<double>("zramComprMib", static_cast<double>(m_psiMem.zramCompressedBytes) / (1024.0 * 1024.0)); }
    double zramMemUsedMib() const { return remoteValue<double>("zramMemUsedMib", static_cast<double>(m_psiMem.zramMemUsedTotalBytes) / (1024.0 * 1024.0)); }
    double zramCompressionRatio() const { return remoteValue<double>("zramCompressionRatio", m_psiMem.zramCompressionRatio); }
    double swapUsedGib() const
    {
        const quint64 used = (m_psiMem.swapTotalBytes > m_psiMem.swapFreeBytes)
            ? (m_psiMem.swapTotalBytes - m_psiMem.swapFreeBytes)
            : 0ULL;
        return remoteValue<double>("swapUsedGib", static_cast<double>(used) / (1024.0 * 1024.0 * 1024.0));
    }
    double swapTotalGib() const { return remoteValue<double>("swapTotalGib", static_cast<double>(m_psiMem.swapTotalBytes) / (1024.0 * 1024.0 * 1024.0)); }
    bool swapAvailable() const { return remoteValue<bool>("swapAvailable", m_psiMem.swapAvailable); }
    bool zramAvailable() const { return remoteValue<bool>("zramAvailable", m_psiMem.zramAvailable); }
    bool zramCompressionAvailable() const { return remoteValue<bool>("zramCompressionAvailable", m_psiMem.zramCompressionAvailable); }
    bool diskThroughputAvailable() const { return remoteValue<bool>("diskThroughputAvailable", m_psiMem.diskThroughputAvailable); }
    double diskReadMibPerSec() const { return remoteValue<double>("diskReadMibPerSec", m_psiMem.diskReadMibPerSec); }
    double diskWriteMibPerSec() const { return remoteValue<double>("diskWriteMibPerSec", m_psiMem.diskWriteMibPerSec); }
    bool netThroughputAvailable() const { return remoteValue<bool>("netThroughputAvailable", m_psiMem.netThroughputAvailable); }
    double netRxMibPerSec() const { return remoteValue<double>("netRxMibPerSec", m_psiMem.netRxMibPerSec); }
    double netTxMibPerSec() const { return remoteValue<double>("netTxMibPerSec", m_psiMem.netTxMibPerSec); }
    QVariantList networkInterfaces() const;
    QStringList selectedNetworkInterfaces() const { return m_selectedNetworkInterfaces; }
    void setSelectedNetworkInterfaces(const QStringList &interfaces);

    bool onAcPower() const { return remoteValue<bool>("onAcPower", m_power.onAcPower); }
    bool batteryPresent() const { return remoteValue<bool>("batteryPresent", m_power.batteryPresent); }
    bool batteryStateAvailable() const { return remoteValue<bool>("batteryStateAvailable", m_power.batteryStateAvailable); }
    bool batteryLevelAvailable() const { return remoteValue<bool>("batteryLevelAvailable", m_power.batteryLevelAvailable); }
    bool batteryHealthAvailable() const { return remoteValue<bool>("batteryHealthAvailable", m_power.batteryHealthAvailable); }
    bool batteryPowerAvailable() const { return remoteValue<bool>("batteryPowerAvailable", m_power.batteryPowerAvailable); }
    QString batteryStatus() const { return remoteValue<QString>("batteryStatus", m_power.batteryStatus); }
    double batteryPercent() const { return remoteValue<double>("batteryPercent", m_power.batteryPercent); }
    double batteryWatts() const { return remoteValue<double>("batteryWatts", m_power.batteryWatts); }
    int batteryTimeRemainingMinutes() const { return remoteValue<int>("batteryTimeRemainingMinutes", m_power.batteryTimeRemainingMinutes); }
    QString batteryTimeRemainingText() const;
    double batteryHealthPercent() const { return remoteValue<double>("batteryHealthPercent", m_power.batteryHealthPercent); }
    bool raplExactAvailable() const { return remoteValue<bool>("raplExactAvailable", m_power.raplExactAvailable); }
    bool packagePowerAvailable() const { return remoteValue<bool>("packagePowerAvailable", m_power.packagePowerAvailable); }
    bool packagePowerEstimated() const { return remoteValue<bool>("packagePowerEstimated", m_power.packagePowerEstimated); }
    double packageWatts() const { return remoteValue<double>("packageWatts", m_power.packageWatts); }
    double pl1LimitWatts() const { return remoteValue<double>("pl1LimitWatts", m_power.pl1LimitWatts); }
    double pl2LimitWatts() const { return remoteValue<double>("pl2LimitWatts", m_power.pl2LimitWatts); }
    bool powerLimitsAvailable() const { return remoteValue<bool>("powerLimitsAvailable", m_power.pl1LimitWatts > 0.0 || m_power.pl2LimitWatts > 0.0); }
    QString activePowerProfile() const { return remoteValue<QString>("activePowerProfile", m_activePowerProfile); }
    QStringList availablePowerProfiles() const { return remoteValue<QStringList>("availablePowerProfiles", m_availablePowerProfiles); }
    QVariantList powerProfileHolds() const { return remoteValue<QVariantList>("powerProfileHolds", m_powerProfileHoldsVariant); }
    QString powerProfileHoldsSummary() const { return remoteValue<QString>("powerProfileHoldsSummary", m_powerProfileHoldsSummary); }
    bool powerProfileServiceAvailable() const { return remoteValue<bool>("powerProfileServiceAvailable", m_powerProfileServiceAvailable); }
    int primaryFanRpm() const { return remoteValue<int>("primaryFanRpm", m_power.primaryFanRpm); }
    bool fanAvailable() const { return remoteValue<bool>("fanAvailable", m_power.fanAvailable); }
    QVariantList fans() const { return remoteValue<QVariantList>("fans", m_fansVariant); }
    double nvmeTempCelsius() const { return remoteValue<double>("nvmeTempCelsius", m_nvmeTemp); }
    double wifiTempCelsius() const { return remoteValue<double>("wifiTempCelsius", m_wifiTemp); }
    bool nvmeTempAvailable() const { return remoteValue<bool>("nvmeTempAvailable", m_nvmeTempAvailable); }
    bool wifiTempAvailable() const { return remoteValue<bool>("wifiTempAvailable", m_wifiTempAvailable); }
    QVariantList nvmeControllers() const { return remoteValue<QVariantList>("nvmeControllers", m_nvmeControllersVariant); }
    bool nvmeWarningActive() const { return remoteValue<bool>("nvmeWarningActive", m_nvmeWarningActive); }

    int verdictSeverity() const { return remoteValue<int>("verdictSeverity", static_cast<int>(m_verdict.severity)); }
    QString verdictHeadline() const { return remoteValue<QString>("verdictHeadline", m_verdict.headline); }
    QString verdictCausalChain() const { return remoteValue<QString>("verdictCausalChain", m_verdict.causalChain); }
    QString verdictRecommendation() const { return remoteValue<QString>("verdictRecommendation", m_verdict.recommendation); }
    QString verdictRuleId() const { return remoteValue<QString>("verdictRuleId", m_verdict.ruleId); }
    double verdictDurationSeconds() const { return remoteValue<double>("verdictDurationSeconds", m_verdict.durationMs / 1000.0); }
    QStringList verdictEvidence() const { return remoteValue<QStringList>("verdictEvidence", m_verdict.evidence); }
    QStringList verdictMissingEvidence() const;
    QString primaryBottleneckDomain() const { return remoteValue<QString>("primaryBottleneckDomain", m_verdict.primaryBottleneckDomain); }
    QStringList verdictContributingSignals() const { return remoteValue<QStringList>("verdictContributingSignals", m_verdict.contributingSignals); }
    QString culpritName() const { return remoteValue<QString>("culpritName", m_verdict.topProcess.name); }
    int culpritPid() const { return remoteValue<int>("culpritPid", m_verdict.topProcess.pid); }
    double culpritCpuPercent() const { return remoteValue<double>("culpritCpuPercent", m_verdict.topProcess.cpuPercent); }
    double culpritRssMb() const { return remoteValue<double>("culpritRssMb", m_verdict.topProcess.rssMegabytes); }
    QString memoryCulpritName() const { return remoteValue<QString>("memoryCulpritName", m_verdict.topMemoryProcess.name); }
    int memoryCulpritPid() const { return remoteValue<int>("memoryCulpritPid", m_verdict.topMemoryProcess.pid); }
    double memoryCulpritRssMb() const { return remoteValue<double>("memoryCulpritRssMb", m_verdict.topMemoryProcess.rssMegabytes); }
    int diskWaitProcessCount() const { return remoteValue<int>("diskWaitProcessCount", m_processRadar.diskWaitProcessCount); }

    QVariantList domainSensors() const { return remoteValue<QVariantList>("domainSensors", m_domainSensorsVariant); }
    QVariantList coreItems() const { return remoteValue<QVariantList>("coreItems", m_coreItemsVariant); }
    QVariantList topProcesses() const { return remoteValue<QVariantList>("topProcesses", m_topProcessesVariant); }
    QVariantList historySamples() const { return m_historySamplesVariant; }
    QVariantMap historySummary() const;
    Q_INVOKABLE QVariantMap historySampleAt(int index) const;
    Q_INVOKABLE int historyIndexAtRatio(double ratio) const;
    Q_INVOKABLE QVariantMap historyPlot(int pixelWidth) const;
    Q_INVOKABLE QString historyPlotJson(int pixelWidth) const;
    Q_INVOKABLE QVariantList historyRecentSeries(const QString &valueKey, const QString &availableKey, int limit) const;
    Q_INVOKABLE QString historyRecentSeriesJson(const QString &valueKey, const QString &availableKey, int limit) const;
    QVariantList incidentHistory() const { return m_incidentHistoryVariant; }
    double historyMaxWatts() const { return remoteValue<double>("historyMaxWatts", m_historyMaxWatts); }
    double historyMaxDiskMibPerSec() const { return remoteValue<double>("historyMaxDiskMibPerSec", m_historyMaxDiskMibPerSec); }
    QVariantMap metricMetadata() const;

    int selectedHistoryIndex() const { return m_selectedHistorySampleId == 0 ? -1 : m_recorder.indexOfSample(m_selectedHistorySampleId); }
    void setSelectedHistoryIndex(int index);
    qulonglong selectedHistorySampleId() const { return m_selectedHistorySampleId; }
    void setSelectedHistorySampleId(qulonglong sampleId);
    bool selectedHistoryExpired() const { return m_selectedHistorySampleId != 0 && m_recorder.indexOfSample(m_selectedHistorySampleId) < 0; }
    QVariantMap selectedHistoryDetail() const;

    int pollIntervalMs() const { return m_timer.interval(); }
    void setPollIntervalMs(int intervalMs);

    int historyRetentionSeconds() const { return m_remoteClient ? m_requestedRetentionSeconds : static_cast<int>(m_recorder.retentionMs() / 1000LL); }
    void setHistoryRetentionSeconds(int seconds);
    double actualHistorySeconds() const;
    bool exportIncludeProcessDetails() const { return m_exportIncludeProcessDetails; }
    void setExportIncludeProcessDetails(bool include);

    bool notificationsEnabled() const { return m_notificationsEnabled; }
    void setNotificationsEnabled(bool enabled);

    bool recordingPaused() const { return m_recordingPaused; }
    void setRecordingPaused(bool paused);

    QString lastActionStatus() const { return m_lastActionStatus; }
    bool lastActionSucceeded() const { return m_lastActionSucceeded; }
    QString lastActionPath() const { return m_lastActionPath; }
    bool powerProfileChangePending() const { return m_powerProfileChangePending; }

    Q_INVOKABLE void refreshNow();
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE bool setPowerProfile(const QString &profile);
    Q_INVOKABLE void launchStudioApp(const QString &pageName = QString(), const QString &sectionName = QString());
    static QString normalizedPageName(const QString &page);
    static QString validatedSectionName(const QString &page, const QString &section);
    Q_INVOKABLE bool openSystemMonitor();
    Q_INVOKABLE bool openReportsFolder();
    Q_INVOKABLE int jumpToPeakThermalSpike();
    Q_INVOKABLE int jumpToWorstStallSpike();
    Q_INVOKABLE QString copyIncidentReportToClipboard();
    Q_INVOKABLE QString exportIncidentReportToFile();
    Q_INVOKABLE void requestTrends(qint64 fromMs, qint64 toMs, int maxPoints = 120, const QStringList &metricIds = QStringList());
    Q_INVOKABLE void loadTrendsRange(const QString &rangeKey);
    Q_INVOKABLE void loadPersistedIncidents(qint64 fromMs, qint64 toMs, int limit = 100);
    Q_INVOKABLE void clearPersistedHistory();
    Q_INVOKABLE QVariantMap processIoStats(int pid) const;
    Q_INVOKABLE QVariantList incidentKernelLogs(const QString &incidentId) const;
    Q_INVOKABLE QVariantMap compareIncidents(const QString &incidentIdA, const QString &incidentIdB) const;

Q_SIGNALS:
    void selectedNetworkInterfacesChanged();
    void exportIncludeProcessDetailsChanged();
    void telemetryUpdated();
    void selectedHistoryChanged();
    void pollIntervalChanged();
    void historyRetentionChanged();
    void notificationsEnabledChanged();
    void recordingPausedChanged();
    void lastActionStatusChanged();
    void collectorPreferencesChanged();
    void temperatureUnitChanged();
    void trendsDataChanged();
    void trendsErrorChanged();
    void persistedIncidentsLoadingChanged();
    void persistedIncidentsErrorChanged();
    void trendsLoadingChanged();
    void trendsRangeChanged();
    void persistedIncidentsChanged();

public:
    Q_PROPERTY(QVariantList allProcesses READ allProcesses NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList storageDevices READ storageDevices NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList gpuDevices READ gpuDevices NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList batteries READ batteries NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantList capabilities READ capabilities NOTIFY telemetryUpdated)
    Q_PROPERTY(QString collectorState READ collectorState NOTIFY telemetryUpdated)
    Q_PROPERTY(bool collectorConnected READ collectorConnected NOTIFY telemetryUpdated)
    Q_PROPERTY(bool backgroundMonitoring READ backgroundMonitoring WRITE setBackgroundMonitoring NOTIFY telemetryUpdated)
    Q_PROPERTY(bool startAtLogin READ startAtLogin WRITE setStartAtLogin NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantMap collectorPreferences READ collectorPreferences NOTIFY collectorPreferencesChanged)
    Q_PROPERTY(QString temperatureUnit READ temperatureUnit WRITE setTemperatureUnit NOTIFY temperatureUnitChanged)
    Q_PROPERTY(QVariantMap trendsData READ trendsData NOTIFY trendsDataChanged)
    Q_PROPERTY(QString trendsError READ trendsError NOTIFY trendsErrorChanged)
    Q_PROPERTY(bool persistedIncidentsLoading READ persistedIncidentsLoading NOTIFY persistedIncidentsLoadingChanged)
    Q_PROPERTY(QString persistedIncidentsError READ persistedIncidentsError NOTIFY persistedIncidentsErrorChanged)
    Q_PROPERTY(bool trendsLoading READ trendsLoading NOTIFY trendsLoadingChanged)
    Q_PROPERTY(QString trendsRange READ trendsRange WRITE setTrendsRange NOTIFY trendsRangeChanged)
    Q_PROPERTY(QVariantList persistedIncidents READ persistedIncidents NOTIFY persistedIncidentsChanged)
    Q_PROPERTY(bool sessionMode READ sessionMode NOTIFY telemetryUpdated)
    Q_PROPERTY(QString sessionPath READ sessionPath NOTIFY telemetryUpdated)
    Q_PROPERTY(bool unsavedInvestigation READ unsavedInvestigation NOTIFY telemetryUpdated)
    Q_PROPERTY(QVariantMap investigationSummary READ investigationSummary NOTIFY telemetryUpdated)
    Q_PROPERTY(QString selectedGpuId READ selectedGpuId WRITE setSelectedGpuId NOTIFY telemetryUpdated)
    Q_PROPERTY(QString selectedStorageId READ selectedStorageId WRITE setSelectedStorageId NOTIFY telemetryUpdated)
    Q_PROPERTY(bool actionPending READ actionPending NOTIFY telemetryUpdated)
    QVariantList allProcesses() const;
    QVariantList storageDevices() const;
    QVariantList gpuDevices() const;
    QVariantList batteries() const;
    QVariantList capabilities() const;
    QString collectorState() const;
    bool collectorConnected() const;
    bool backgroundMonitoring() const { return m_remoteValues.value(QStringLiteral("backgroundMonitoring")).toBool(); }
    bool startAtLogin() const { return m_remoteValues.value(QStringLiteral("startAtLogin")).toBool(); }
    void setBackgroundMonitoring(bool enabled);
    void setStartAtLogin(bool enabled);
    QVariantMap collectorPreferences() const { return m_collectorPreferences; }
    QString temperatureUnit() const { return m_temperatureUnit; }
    void setTemperatureUnit(const QString &unit);
    Q_INVOKABLE void updateCollectorPreferences(const QVariantMap &prefs);
    QVariantMap trendsData() const { return m_trendsData; }
    QString trendsError() const { return m_trendsError; }
    bool persistedIncidentsLoading() const { return m_persistedIncidentsLoading; }
    QString persistedIncidentsError() const { return m_persistedIncidentsError; }
    bool trendsLoading() const { return m_trendsLoading; }
    QString trendsRange() const { return m_trendsRange; }
    void setTrendsRange(const QString &range);
    QVariantList persistedIncidents() const { return m_persistedIncidents; }
    bool sessionMode() const { return m_sessionMode; }
    QString sessionPath() const { return m_sessionPath; }
    bool unsavedInvestigation() const { return m_unsavedInvestigation; }
    QVariantMap investigationSummary() const { return m_investigationSummary; }
    QString selectedGpuId() const { return m_selectedGpuId; }
    QString selectedStorageId() const { return m_selectedStorageId; }
    void setSelectedGpuId(const QString &id);
    void setSelectedStorageId(const QString &id);
    bool actionPending() const { return m_actionPending; }
    Q_INVOKABLE void reconnectCollector();
    Q_INVOKABLE void markProblemNow();
    Q_INVOKABLE void markProblemAndLaunch();
    Q_INVOKABLE void selectIncident(const QString &id);
    Q_INVOKABLE void launchIncident(const QString &id);
    Q_INVOKABLE void selectHistoryRange(qulonglong first, qulonglong last);
    Q_INVOKABLE void returnToLive();
    Q_INVOKABLE void saveSession(const QString &path);
    Q_INVOKABLE void openSession(const QString &path);
    Q_INVOKABLE QString exportSelectedIncident();
    Q_INVOKABLE QString exportEntireRecording();
    Q_INVOKABLE void snoozeNotifications();
    Q_INVOKABLE void quitMonitoring();
    Q_INVOKABLE bool canQuitWithoutSaving() const { return !m_unsavedInvestigation; }
    void handleSystemResume();

private Q_SLOTS:
    void receiveCollectorFrame(const QByteArray &data);
    void connectCollector();
private:
    QVariant remoteTelemetryValue(const char *key) const;
    QVariantList remoteCollection(const char *key) const;
    QVariantMap sampleForView(const FlightSample &sample, int index) const;
    template<class T> T remoteValue(const char *key, const T &local) const {
        return m_remoteClient ? remoteTelemetryValue(key).value<T>() : local;
    }
    void callCollector(const QString &method, const QVariantList &arguments = {},
                       std::function<void(const QByteArray &)> completion = {},
                       std::function<void(const QString &)> failure = {});
    void applyCollectorPreferences(const QVariantMap &prefs);
    void subscribeCollector();
    void refreshProcesses();
    void fetchCollectorHistory(qulonglong afterId = 0);
    void appendRemoteSample(const QVariantMap &map);
    void showSession(const SessionSnapshot &session, const QString &path, bool unsaved);
    void rebuildHistoryView();
    void exportRecorderAsync(const FlightRecorder &recorder, const QVariantMap &selection = {});
    QVariantMap processToMap(const CulpritProcess &process) const;
    void setLastActionStatus(const QString &status);
    void setActionResult(bool succeeded, const QString &status, const QString &path = QString());
    void refreshPowerProfileProperties();
    static QVariantMap sampleToVariantMap(const FlightSample &s, int index);

    QString m_clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QElapsedTimer m_lastLaunchAttempt;
    mutable qint64 m_lastPacketProcessMs = -1;
    quint64 m_investigationGeneration = 0;
    quint64 m_connectionGeneration = 0;
    int m_requestedRetentionSeconds = 300;
    QVariantList m_pendingSamples;
    bool m_remoteClient = false;
    bool m_connecting = false;
    bool m_syncing = false;
    bool m_sessionMode = false;
    bool m_unsavedInvestigation = false;
    bool m_actionPending = false;
    QString m_sessionPath;
    QString m_captureId;
    QString m_remoteCaptureId;
    QString m_selectedGpuId;
    QString m_selectedStorageId;
    QString m_connectionState = QStringLiteral("Connecting");
    QVariantMap m_remoteValues;
    QVariantMap m_investigationSummary;
    QVariantMap m_collectorPreferences;
    QString m_temperatureUnit = QStringLiteral("C");
    quint64 m_preferencesRequestId = 0;
    bool m_preferencesUpdating = false;
    bool m_openedSession = false;
    QHash<QString, SessionSnapshot> m_markedInvestigations;
    QStringList m_markedOrder;
    SessionSnapshot m_sourceSession;
    SessionSnapshot m_session;
    FlightRecorder m_liveRecorder{7201};
    QTimer m_reconnectTimer;
    QElapsedTimer m_receivedAt;
    QThreadPool m_filePool;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QList<GpuSnapshot> m_gpus;
    QVariantList m_allProcesses;
    QVariantList m_processObservations;
    SysfsReader m_sysfsReader;
    PsiMemorySampler m_psiSampler;
    CpuSocSampler m_cpuSampler;
    CpuSocSampler m_processSampler;
    PowerFlowSampler m_powerSampler;
    FlightRecorder m_recorder;
    QDBusConnection m_powerProfileBus;

    QTimer m_timer;
    QTimer m_powerProfileRefreshTimer;
    QTimer m_processTimer;
    QElapsedTimer m_uptime;
    qint64 m_lastBoottimeMs = 0;
    qint64 m_lastSampleWallTimeMs = 0;
    qint64 m_lastProcessScanBoottimeMs = 0;
    bool m_gapPending = false;
    bool m_notificationsEnabled = true;
    bool m_recordingPaused = false;
    bool m_exportIncludeProcessDetails = false;
    QStringList m_selectedNetworkInterfaces;
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
    QVariantList m_powerProfileHoldsVariant;
    QString m_powerProfileHoldsSummary;
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
    QVariantList m_fansVariant;
    QVariantList m_nvmeControllersVariant;
    bool m_nvmeWarningActive = false;
    QVariantList m_topProcessesVariant;
    struct HistoryPlotSample {
        quint64 sampleId = 0;
        qint64 timestamp = 0;
        double values[7]{};
        bool available[7]{};
        bool gap = false;
        bool throttled = false;
        int severity = 0;
    };
    mutable QList<HistoryPlotSample> m_historyPlotSamples;
    QVariantList m_historySamplesVariant;
    QVariantList m_incidentHistoryVariant;
    double m_historyMaxWatts = 65.0;
    double m_historyMaxDiskMibPerSec = 100.0;
    int m_selectedHistoryIndex = -1;
    QVariantMap m_trendsData;
    QString m_trendsError;
    QString m_persistedIncidentsError;
    bool m_persistedIncidentsLoading = false;
    quint64 m_trendsRequestId = 0;
    quint64 m_incidentsRequestId = 0;
    bool m_trendsLoading = false;
    QString m_trendsRange = QStringLiteral("1h");
    QVariantList m_persistedIncidents;
};

} // namespace LoofiPulse
