#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>
#include <QtGlobal>

namespace LoofiPulse
{

struct SysfsRootConfig
{
    QString sysRoot = QStringLiteral("/sys");
    QString procRoot = QStringLiteral("/proc");
};

enum class PhysicalDomain
{
    CpuSoc = 0,
    Graphics,
    MemoryZram,
    StorageNvme,
    NetworkWireless,
    PowerBattery,
    ChassisCooling
};

enum class VerdictSeverity
{
    Nominal = 0,
    Elevated = 1,
    Bottleneck = 2,
    CriticalThrottle = 3,
    Unknown = 4
};

struct DomainSensorReading
{
    QString id;
    QString chipName;
    QString label;
    QString sysfsPath;
    PhysicalDomain domain = PhysicalDomain::ChassisCooling;
    bool available = false;
    bool limitsAvailable = false;
    double currentCelsius = 0.0;
    double maxCelsius = 100.0;
    double critCelsius = 100.0;
    double headroomCelsius = 100.0;
    double velocityCelsiusPerSec = 0.0;
};

struct CoreTelemetry
{
    int coreId = 0;
    bool loadAvailable = false;
    double loadPercent = 0.0;
    int currentFreqMhz = 0;
    int maxFreqMhz = 0;
    double tempCelsius = 0.0;
    qint64 throttleCount = 0;
};

struct CpuSocSnapshot
{
    bool loadAvailable = false;
    bool loadWarmedUp = false;
    bool temperatureAvailable = false;
    bool thermalLimitAvailable = false;
    bool frequencyAvailable = false;
    bool throttleCountersAvailable = false;
    double totalLoadPercent = 0.0;
    double packageTempCelsius = 0.0;
    double packageMaxCelsius = 100.0;
    double thermalHeadroomCelsius = 100.0;
    double thermalVelocityCPerSec = 0.0;
    int avgFreqMhz = 0;
    int maxFreqMhz = 0;
    int peakCoreFreqMhz = 0;
    qint64 totalThrottleEvents = 0;
    qint64 deltaThrottleEvents = 0;
    QList<CoreTelemetry> cores;
};

struct GpuSnapshot
{
    bool available = false;
    bool frequencyAvailable = false;
    bool busyAvailable = false;
    bool temperatureAvailable = false;
    bool powerAvailable = false;
    bool vramAvailable = false;
    QString sysfsPath;
    QString driverName;
    int currentFreqMhz = 0;
    int boostFreqMhz = 0;
    double busyPercent = 0.0;
    double tempCelsius = 0.0;
    double vramUsedMegabytes = 0.0;
    double vramTotalMegabytes = 0.0;
    double powerWatts = 0.0;
};

struct PsiMetric
{
    bool available = false;
    bool someAvailable = false;
    bool fullAvailable = false;
    double someAvg10 = 0.0;
    double someAvg60 = 0.0;
    double fullAvg10 = 0.0;
    double fullAvg60 = 0.0;
    quint64 someTotalUs = 0;
    quint64 fullTotalUs = 0;
    double deltaStallMsPerSec = 0.0;
};

struct PsiAndMemorySnapshot
{
    PsiMetric cpuPsi;
    PsiMetric memoryPsi;
    PsiMetric ioPsi;
    bool ramAvailable = false;
    bool zramAvailable = false;
    bool zramCompressionAvailable = false;
    bool swapAvailable = false;
    bool responsivenessAvailable = false;
    bool sampledAtLeastOnce = false;

    quint64 ramTotalBytes = 0;
    quint64 ramAvailableBytes = 0;
    quint64 ramUsedBytes = 0;
    double ramUsedPercent = 0.0;

    bool zramActive = false;
    quint64 zramOrigBytes = 0;
    quint64 zramCompressedBytes = 0;
    quint64 zramMemUsedTotalBytes = 0;
    double zramCompressionRatio = 1.0;

    quint64 swapTotalBytes = 0;
    quint64 swapFreeBytes = 0;
    double responsivenessScore = 0.0;

    bool diskThroughputAvailable = false;
    double diskReadMibPerSec = 0.0;
    double diskWriteMibPerSec = 0.0;

    bool netThroughputAvailable = false;
    double netRxMibPerSec = 0.0;
    double netTxMibPerSec = 0.0;
};

struct PowerFlowSnapshot
{
    bool batteryStateAvailable = false;
    bool batteryLevelAvailable = false;
    bool batteryHealthAvailable = false;
    bool batteryPowerAvailable = false;
    bool fanAvailable = false;
    bool packagePowerAvailable = false;
    bool packagePowerEstimated = false;
    bool onAcPower = true;
    bool batteryPresent = false;
    QString batteryStatus;
    double batteryPercent = 100.0;
    double batteryWatts = 0.0;
    double batteryVoltageVolts = 0.0;
    int batteryTimeRemainingMinutes = -1;
    double batteryHealthPercent = 100.0;

    bool raplExactAvailable = false;
    bool hwmonPowerAvailable = false;
    double packageWatts = 0.0;
    double pl1LimitWatts = 0.0;
    double pl2LimitWatts = 0.0;

    QString activePowerProfile = QStringLiteral("balanced");
    QStringList availablePowerProfiles;
    int primaryFanRpm = 0;
};

struct CulpritProcess
{
    int pid = 0;
    QString name;
    double cpuPercent = 0.0;
    double rssMegabytes = 0.0;
    QString state;
    bool inDiskWait = false;
};

struct ProcessRadarSnapshot
{
    CulpritProcess topCpuProcess;
    CulpritProcess topMemoryProcess;
    CulpritProcess topDiskWaitProcess;
    int diskWaitProcessCount = 0;
    QList<CulpritProcess> topProcesses;
};

struct CausalVerdict
{
    VerdictSeverity severity = VerdictSeverity::Unknown;
    QString headline;
    QString causalChain;
    QString recommendation;
    QString primaryBottleneckDomain;
    QStringList contributingSignals;
    CulpritProcess topProcess;
    CulpritProcess topMemoryProcess;
};

struct FlightSample
{
    quint64 sampleId = 0;
    qint64 timestampMs = 0;
    qint64 monotonicMs = 0;
    qint64 elapsedSincePreviousMs = 0;
    double cpuTempCelsius = 0.0;
    double nvmeTempCelsius = 0.0;
    double wifiTempCelsius = 0.0;
    double cpuLoadPercent = 0.0;
    int cpuFreqMhz = 0;
    double gpuBusyPercent = 0.0;
    double gpuTempCelsius = 0.0;
    int gpuFreqMhz = 0;
    double gpuPowerWatts = 0.0;
    double gpuVramUsedMegabytes = 0.0;
    double gpuVramTotalMegabytes = 0.0;
    double psiCpuSome10 = 0.0;
    double psiCpuSome60 = 0.0;
    double psiMemSome10 = 0.0;
    double psiMemSome60 = 0.0;
    double psiMemFull10 = 0.0;
    double psiMemFull60 = 0.0;
    double psiIoSome10 = 0.0;
    double psiIoSome60 = 0.0;
    double psiIoFull10 = 0.0;
    double psiIoFull60 = 0.0;
    double psiCpuStallMsPerSec = 0.0;
    double psiMemStallMsPerSec = 0.0;
    double psiIoStallMsPerSec = 0.0;
    double responsivenessScore = 100.0;
    double systemWatts = 0.0;
    double packageWatts = 0.0;
    double batteryWatts = 0.0;
    double batteryPercent = 0.0;
    double batteryHealthPercent = 0.0;
    double pl1LimitWatts = 0.0;
    double pl2LimitWatts = 0.0;
    int fanRpm = 0;
    double ramUsedPercent = 0.0;
    quint64 ramUsedBytes = 0;
    quint64 ramTotalBytes = 0;
    quint64 swapUsedBytes = 0;
    quint64 swapTotalBytes = 0;
    double zramCompressionRatio = 1.0;
    quint64 zramOriginalBytes = 0;
    quint64 zramCompressedBytes = 0;
    quint64 zramMemoryUsedBytes = 0;
    double diskReadMibPerSec = 0.0;
    double diskWriteMibPerSec = 0.0;
    double netRxMibPerSec = 0.0;
    double netTxMibPerSec = 0.0;
    qint64 throttleEventCount = 0;
    qint64 newThrottleEvents = 0;
    bool batteryPresent = false;
    bool onAcPower = true;
    QString activePowerProfile;
    bool throttled = false;
    VerdictSeverity severity = VerdictSeverity::Nominal;
    QString verdictHeadline;
    QString culpritName;
    int culpritPid = 0;
    double culpritCpuPercent = 0.0;
    double culpritRssMb = 0.0;
    QVariantMap metricQuality;
};

} // namespace LoofiPulse
