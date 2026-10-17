#pragma once

#include "telemetrytypes.h"

namespace LoofiPulse
{

class PsiMemorySampler
{
public:
    explicit PsiMemorySampler(SysfsRootConfig config = {});

    void setRootConfig(const SysfsRootConfig &config);
    PsiAndMemorySnapshot sample(qint64 elapsedMs);

    static PsiMetric parsePsiContent(const QString &content, quint64 prevTotalUs, qint64 elapsedMs);

private:
    SysfsRootConfig m_config;
    quint64 m_prevCpuSomeUs = 0;
    quint64 m_prevMemSomeUs = 0;
    quint64 m_prevIoSomeUs = 0;
};

} // namespace LoofiPulse
