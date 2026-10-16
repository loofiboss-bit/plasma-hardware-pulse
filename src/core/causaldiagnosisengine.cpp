#include "causaldiagnosisengine.h"

namespace LoofiPulse
{

CausalVerdict CausalDiagnosisEngine::evaluate(
    const CpuSocSnapshot &cpu,
    const GpuSnapshot &gpu,
    const PsiAndMemorySnapshot &psiMem,
    const PowerFlowSnapshot &power,
    const QList<DomainSensorReading> &domains,
    const CulpritProcess &culprit,
    const CulpritProcess &memoryCulprit)
{
    CausalVerdict verdict;
    verdict.topProcess = culprit;
    verdict.topMemoryProcess = !memoryCulprit.name.isEmpty() ? memoryCulprit : culprit;

    double nvmeTemp = 0.0;
    for (const DomainSensorReading &d : domains) {
        if (d.domain == PhysicalDomain::StorageNvme && d.currentCelsius > nvmeTemp) {
            nvmeTemp = d.currentCelsius;
        }
    }

    const QString culpritTag = (!culprit.name.isEmpty() && culprit.cpuPercent >= 3.0)
        ? QStringLiteral("%1 (PID %2, %3% CPU)")
              .arg(culprit.name)
              .arg(culprit.pid)
              .arg(culprit.cpuPercent, 0, 'f', 1)
        : QStringLiteral("background activity");

    const CulpritProcess &memProc = (!memoryCulprit.name.isEmpty() && memoryCulprit.rssMegabytes >= 16.0)
        ? memoryCulprit
        : culprit;
    const QString memCulpritTag = (!memProc.name.isEmpty() && (memProc.rssMegabytes >= 16.0 || memProc.cpuPercent >= 3.0))
        ? (memProc.rssMegabytes >= 16.0
               ? QStringLiteral("%1 (PID %2, %3 MiB RSS)")
                     .arg(memProc.name)
                     .arg(memProc.pid)
                     .arg(memProc.rssMegabytes, 0, 'f', 0)
               : culpritTag)
        : culpritTag;

    // 1. Check active Thermal Throttling or critical thermal headroom
    if (cpu.deltaThrottleEvents > 0 || (cpu.packageTempCelsius > 0.0 && cpu.thermalHeadroomCelsius <= 6.0)) {
        verdict.severity = VerdictSeverity::CriticalThrottle;
        verdict.primaryBottleneckDomain = QStringLiteral("CPU & SoC Thermal Limit");
        verdict.headline = QStringLiteral("Thermal Throttling Active — %1°C (%2°C headroom)")
                               .arg(cpu.packageTempCelsius, 0, 'f', 0)
                               .arg(cpu.thermalHeadroomCelsius, 0, 'f', 0);
        verdict.causalChain = QStringLiteral("%1 ➔ %2 W power ➔ +%3 throttle-events (%4 MHz) ➔ %5°C Package")
                                  .arg(culpritTag)
                                  .arg(power.packageWatts, 0, 'f', 1)
                                  .arg(cpu.deltaThrottleEvents)
                                  .arg(cpu.avgFreqMhz)
                                  .arg(cpu.packageTempCelsius, 0, 'f', 1);
        verdict.recommendation = QStringLiteral("Switch Power Profile to 'balanced' or 'power-saver' to lower PL1/PL2 limits and restore thermal headroom.");
        return verdict;
    }

    // 2. Check Memory PSI Stall (actual memory reclaim lag)
    if (psiMem.memoryPsi.someAvg10 >= 5.0 || psiMem.memoryPsi.fullAvg10 >= 2.0) {
        verdict.severity = VerdictSeverity::Bottleneck;
        verdict.primaryBottleneckDomain = QStringLiteral("Memory & ZRAM Reclaim");
        verdict.headline = QStringLiteral("Memory Stall Detected (PSI some=%1%, full=%2%)")
                               .arg(psiMem.memoryPsi.someAvg10, 0, 'f', 1)
                               .arg(psiMem.memoryPsi.fullAvg10, 0, 'f', 1);
        verdict.causalChain = QStringLiteral("RAM %1% used ➔ ZRAM %2× compression ➔ %3 ms/s reclaim stall (%4)")
                                  .arg(psiMem.ramUsedPercent, 0, 'f', 0)
                                  .arg(psiMem.zramCompressionRatio, 0, 'f', 1)
                                  .arg(psiMem.memoryPsi.deltaStallMsPerSec, 0, 'f', 0)
                                  .arg(memCulpritTag);
        verdict.recommendation = QStringLiteral("Threads are stalling on memory compaction/reclaim. Close or terminate memory-heavy processes to eliminate micro-stutters.");
        return verdict;
    }

    // 3. Check I/O PSI Stall or NVMe thermal stress
    if (psiMem.ioPsi.fullAvg10 >= 8.0 || nvmeTemp >= 70.0) {
        verdict.severity = VerdictSeverity::Bottleneck;
        verdict.primaryBottleneckDomain = QStringLiteral("Storage & NVMe I/O");
        verdict.headline = QStringLiteral("Disk I/O Bottleneck (PSI full=%1%, NVMe %2°C)")
                               .arg(psiMem.ioPsi.fullAvg10, 0, 'f', 1)
                               .arg(nvmeTemp, 0, 'f', 0);
        verdict.causalChain = QStringLiteral("%1 ➔ I/O queue stall %2% ➔ NVMe %3°C ➔ Responsiveness %4%")
                                  .arg(culpritTag)
                                  .arg(psiMem.ioPsi.someAvg10, 0, 'f', 1)
                                  .arg(nvmeTemp, 0, 'f', 1)
                                  .arg(psiMem.responsivenessScore, 0, 'f', 0);
        verdict.recommendation = QStringLiteral("System is waiting on synchronous disk reads/writes. Check background indexing or heavy I/O jobs.");
        return verdict;
    }

    // 4. Check GPU Saturation or GPU Thermal Bottleneck
    if (gpu.available && (gpu.tempCelsius >= 88.0 || (gpu.busyPercent >= 92.0 && gpu.tempCelsius >= 75.0))) {
        verdict.severity = VerdictSeverity::Bottleneck;
        verdict.primaryBottleneckDomain = QStringLiteral("Graphics & Display (GPU)");
        verdict.headline = QStringLiteral("GPU Compute / Thermal Saturation — %1% Busy (%2°C)")
                               .arg(gpu.busyPercent, 0, 'f', 0)
                               .arg(gpu.tempCelsius, 0, 'f', 0);
        verdict.causalChain = QStringLiteral("%1 ➔ GPU %2% @ %3 MHz (%4) ➔ %5°C GPU")
                                  .arg(culpritTag)
                                  .arg(gpu.busyPercent, 0, 'f', 0)
                                  .arg(gpu.currentFreqMhz)
                                  .arg(gpu.driverName.isEmpty() ? QStringLiteral("DRM") : gpu.driverName)
                                  .arg(gpu.tempCelsius, 0, 'f', 1);
        verdict.recommendation = QStringLiteral("Graphics pipeline is near saturation or thermal limit. Lower rendering load or frame-rate cap if frame pacing drops.");
        return verdict;
    }

    // 5. Check rapid thermal climb (dT/dt) or elevated CPU/GPU load
    if (cpu.thermalVelocityCPerSec >= 0.8 || (cpu.packageTempCelsius > 0.0 && cpu.thermalHeadroomCelsius < 20.0) || cpu.totalLoadPercent >= 70.0 || (gpu.available && gpu.busyPercent >= 85.0)) {
        verdict.severity = VerdictSeverity::Elevated;
        verdict.primaryBottleneckDomain = (gpu.available && gpu.busyPercent >= 85.0 && cpu.totalLoadPercent < 70.0)
            ? QStringLiteral("Graphics & Display (GPU)")
            : QStringLiteral("CPU Boost & Thermal Dynamics");
        const QString velPrefix = (cpu.thermalVelocityCPerSec >= 0.2)
            ? QStringLiteral("Thermal Climb (+%1°C/s)").arg(cpu.thermalVelocityCPerSec, 0, 'f', 1)
            : QStringLiteral("Elevated Load");
        verdict.headline = QStringLiteral("%1 — %2°C (%3°C headroom remaining)")
                               .arg(velPrefix)
                               .arg(cpu.packageTempCelsius, 0, 'f', 0)
                               .arg(cpu.thermalHeadroomCelsius, 0, 'f', 0);
        verdict.causalChain = QStringLiteral("%1 ➔ CPU %2% @ %3 MHz ➔ ~%4 W ➔ %5°C Package")
                                  .arg(culpritTag)
                                  .arg(cpu.totalLoadPercent, 0, 'f', 0)
                                  .arg(cpu.avgFreqMhz)
                                  .arg(power.packageWatts, 0, 'f', 1)
                                  .arg(cpu.packageTempCelsius, 0, 'f', 1);
        verdict.recommendation = QStringLiteral("No active throttling yet, but thermal headroom is narrowing under sustained boost load.");
        return verdict;
    }

    // 6. Nominal state
    verdict.severity = VerdictSeverity::Nominal;
    verdict.primaryBottleneckDomain = QStringLiteral("No Bottleneck");
    verdict.headline = QStringLiteral("Nominal — %1°C thermal headroom, %2% responsiveness")
                           .arg(cpu.thermalHeadroomCelsius, 0, 'f', 0)
                           .arg(psiMem.responsivenessScore, 0, 'f', 1);
    verdict.causalChain = QStringLiteral("CPU %1% (%2 MHz) ➔ ~%3 W (%4) ➔ PSI stall %5% ➔ ZRAM %6×")
                              .arg(cpu.totalLoadPercent, 0, 'f', 0)
                              .arg(cpu.avgFreqMhz)
                              .arg(power.packageWatts, 0, 'f', 1)
                              .arg(power.activePowerProfile)
                              .arg(psiMem.cpuPsi.someAvg10, 0, 'f', 2)
                              .arg(psiMem.zramCompressionRatio, 0, 'f', 1);
    verdict.recommendation = QStringLiteral("All physical domains are operating with healthy thermal headroom and minimal PSI stall.");
    return verdict;
}

} // namespace LoofiPulse
