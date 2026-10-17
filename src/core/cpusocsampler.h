#pragma once

#include "telemetrytypes.h"

#include <QHash>
#include <QList>

namespace LoofiPulse
{

class CpuSocSampler
{
public:
    explicit CpuSocSampler(SysfsRootConfig config = {});

    void setRootConfig(const SysfsRootConfig &config);

    CpuSocSnapshot sampleCpu(const QList<DomainSensorReading> &domainReadings, qint64 elapsedMs = 1000);
    GpuSnapshot sampleGpu(const QList<DomainSensorReading> &domainReadings);
    CulpritProcess sampleTopCulpritProcess();
    ProcessRadarSnapshot sampleProcessRadar(int maxTopProcesses = 5);

private:
    struct CpuJiffies
    {
        quint64 active = 0;
        quint64 total = 0;
    };

    struct ProcSample
    {
        quint64 ticks = 0;
        QString comm;
        double rssMb = 0.0;
    };

    SysfsRootConfig m_config;
    QHash<int, CpuJiffies> m_prevCoreJiffies;
    CpuJiffies m_prevTotalJiffies;
    QHash<QString, qint64> m_prevThrottleCounters;

    QHash<int, quint64> m_prevProcTicks;
    quint64 m_prevSystemTotalTicks = 0;
};

} // namespace LoofiPulse
