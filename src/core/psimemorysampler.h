#pragma once

#include "telemetrytypes.h"
#include <QHash>

namespace LoofiPulse
{

class PsiMemorySampler
{
public:
    explicit PsiMemorySampler(SysfsRootConfig config = {});

    void setRootConfig(const SysfsRootConfig &config);
    void setIncludedNetworkInterfaces(const QStringList &interfaces);
    PsiAndMemorySnapshot sample(qint64 elapsedMs);

    static PsiMetric parsePsiContent(const QString &content, quint64 prevTotalUs, qint64 elapsedMs);

private:
    SysfsRootConfig m_config;
    quint64 m_prevCpuSomeUs = 0;
    quint64 m_prevMemSomeUs = 0;
    quint64 m_prevIoSomeUs = 0;
    struct Counters { quint64 received = 0; quint64 sent = 0; };
    QHash<QString, Counters> m_diskCounters;
    QHash<QString, Counters> m_netCounters;
    QStringList m_includedNetworkInterfaces;

};

} // namespace LoofiPulse
