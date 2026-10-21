#pragma once

#include "telemetrytypes.h"

namespace LoofiPulse
{

class CausalDiagnosisEngine
{
public:
    static CausalVerdict evaluate(
        const CpuSocSnapshot &cpu,
        const GpuSnapshot &gpu,
        const PsiAndMemorySnapshot &psiMem,
        const PowerFlowSnapshot &power,
        const QList<DomainSensorReading> &domains,
        const CulpritProcess &culprit,
        const CulpritProcess &memoryCulprit = {});

    static CausalVerdict evaluateDevices(
        const CpuSocSnapshot &cpu,
        const QList<GpuSnapshot> &gpus,
        const PsiAndMemorySnapshot &psiMem,
        const PowerFlowSnapshot &power,
        const QList<DomainSensorReading> &domains,
        const CulpritProcess &culprit,
        const CulpritProcess &memoryCulprit = {});
};

class VerdictStabilizer
{
public:
    VerdictStabilizer();
    CausalVerdict update(const CausalVerdict &candidate, qint64 monotonicMs);
    const CausalVerdict &current() const { return m_current; }

private:
    CausalVerdict m_current;
    QString m_pendingRuleId;
    VerdictSeverity m_pendingSeverity = VerdictSeverity::Unknown;
    qint64 m_pendingSinceMs = 0;
};

} // namespace LoofiPulse
