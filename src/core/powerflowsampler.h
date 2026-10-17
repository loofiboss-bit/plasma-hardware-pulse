#pragma once

#include "telemetrytypes.h"

namespace LoofiPulse
{

class PowerFlowSampler
{
public:
    explicit PowerFlowSampler(SysfsRootConfig config = {});

    void setRootConfig(const SysfsRootConfig &config);
    PowerFlowSnapshot sample(qint64 elapsedMs, double cpuLoadPercent, int avgFreqMhz, int maxFreqMhz);

private:
    SysfsRootConfig m_config;
    quint64 m_prevRaplEnergyUj = 0;
    bool m_hasPrevRapl = false;
};

} // namespace LoofiPulse
