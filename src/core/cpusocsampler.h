#pragma once

#include "telemetrytypes.h"

#include <QHash>
#include <QElapsedTimer>
#include <QList>

namespace LoofiPulse
{

class CpuSocSampler
{
public:
    explicit CpuSocSampler(SysfsRootConfig config = {});

    void setRootConfig(const SysfsRootConfig &config);

    CpuSocSnapshot sampleCpu(const QList<DomainSensorReading> &domainReadings, qint64 elapsedMs = 1000);
    QList<GpuSnapshot> sampleGpus(const QList<DomainSensorReading> &domainReadings);
    GpuSnapshot sampleGpu(const QList<DomainSensorReading> &domainReadings);
    CulpritProcess sampleTopCulpritProcess();
    ProcessRadarSnapshot sampleProcessRadar(int maxTopProcesses = 5);
    void resetBaselines();

private:
    struct CpuJiffies
    {
        quint64 active = 0;
        quint64 total = 0;
    };

    struct ProcSample
    {
        quint64 ticks = 0;
        quint64 startTimeTicks = 0;
        QString comm;
        double rssMb = 0.0;
    };

    SysfsRootConfig m_config;
    QHash<int, CpuJiffies> m_prevCoreJiffies;
    CpuJiffies m_prevTotalJiffies;
    QHash<QString, qint64> m_prevThrottleCounters;

    QHash<int, ProcSample> m_prevProcTicks;
    quint64 m_prevSystemTotalTicks = 0;
    QHash<quint32, QString> m_ownerNames;
    QElapsedTimer m_ownerMetadataAge;
    QHash<QString, QString> m_gpuDrivers;
    QElapsedTimer m_gpuMetadataAge;
};

} // namespace LoofiPulse
