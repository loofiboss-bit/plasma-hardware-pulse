#pragma once

#include <QList>
#include <QString>
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
    CriticalThrottle = 3
};

struct DomainSensorReading
{
    QString id;
    QString chipName;
    QString label;
    PhysicalDomain domain = PhysicalDomain::ChassisCooling;
    double currentCelsius = 0.0;
    double maxCelsius = 100.0;
    double critCelsius = 100.0;
    double headroomCelsius = 100.0;
    double velocityCelsiusPerSec = 0.0;
};

struct CoreTelemetry
{
    int coreId = 0;
    double loadPercent = 0.0;
    int currentFreqMhz = 0;
    int maxFreqMhz = 0;
    double tempCelsius = 0.0;
    qint64 throttleCount = 0;
};

struct CpuSocSnapshot
{
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
    double responsivenessScore = 100.0;
};

struct PowerFlowSnapshot
{
    bool onAcPower = true;
    bool batteryPresent = false;
    QString batteryStatus;
    double batteryPercent = 100.0;
    double batteryWatts = 0.0;
    double batteryVoltageVolts = 0.0;
    int batteryTimeRemainingMinutes = -1;
    double batteryHealthPercent = 100.0;

    bool raplExactAvailable = false;
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
};

struct ProcessRadarSnapshot
{
    CulpritProcess topCpuProcess;
    CulpritProcess topMemoryProcess;
    QList<CulpritProcess> topProcesses;
};

struct CausalVerdict
{
    VerdictSeverity severity = VerdictSeverity::Nominal;
    QString headline;
    QString causalChain;
    QString recommendation;
    QString primaryBottleneckDomain;
    CulpritProcess topProcess;
    CulpritProcess topMemoryProcess;
};

struct FlightSample
{
    qint64 timestampMs = 0;
    double cpuTempCelsius = 0.0;
    double nvmeTempCelsius = 0.0;
    double wifiTempCelsius = 0.0;
    double cpuLoadPercent = 0.0;
    int cpuFreqMhz = 0;
    double gpuBusyPercent = 0.0;
    double gpuTempCelsius = 0.0;
    double psiCpuSome10 = 0.0;
    double psiMemSome10 = 0.0;
    double psiIoFull10 = 0.0;
    double responsivenessScore = 100.0;
    double systemWatts = 0.0;
    bool throttled = false;
    VerdictSeverity severity = VerdictSeverity::Nominal;
    QString verdictHeadline;
    QString culpritName;
    int culpritPid = 0;
    double culpritCpuPercent = 0.0;
    double culpritRssMb = 0.0;
};

} // namespace LoofiPulse
