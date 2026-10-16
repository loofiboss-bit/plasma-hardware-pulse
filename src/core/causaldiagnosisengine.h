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
};

} // namespace LoofiPulse
