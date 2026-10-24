#include "causaldiagnosisengine.h"

#include <algorithm>

namespace LoofiPulse
{

CausalVerdict CausalDiagnosisEngine::evaluate(
    const CpuSocSnapshot &cpu,
    const GpuSnapshot &gpu,
    const PsiAndMemorySnapshot &psiMem,
    const PowerFlowSnapshot &power,
    const QList<DomainSensorReading> &domains,
    const CulpritProcess &cpuProcess,
    const CulpritProcess &memoryProcess)
{
    return evaluateDevices(cpu, {gpu}, psiMem, power, domains, cpuProcess, memoryProcess);
}

CausalVerdict CausalDiagnosisEngine::evaluateDevices(
    const CpuSocSnapshot &cpu,
    const QList<GpuSnapshot> &gpus,
    const PsiAndMemorySnapshot &psiMem,
    const PowerFlowSnapshot &power,
    const QList<DomainSensorReading> &domains,
    const CulpritProcess &cpuProcess,
    const CulpritProcess &memoryProcess)
{
    CausalVerdict verdict;
    verdict.topProcess = cpuProcess;
    verdict.topMemoryProcess = memoryProcess;
    const bool cpuLoadValid = cpu.loadAvailable && cpu.loadWarmedUp;

    QList<CausalVerdict> matches;
    const auto addCandidate = [&matches, &cpuProcess, &memoryProcess](
                                  const QString &ruleId,
                                  VerdictSeverity severity,
                                  const QString &domain,
                                  const QString &headline,
                                  const QString &details,
                                  const QString &recommendation) {
        CausalVerdict v;
        v.ruleId = ruleId;
        v.evidence.append(details);
        v.severity = severity;
        v.primaryBottleneckDomain = domain;
        v.headline = headline;
        v.causalChain = details;
        v.recommendation = recommendation;
        v.topProcess = cpuProcess;
        v.topMemoryProcess = memoryProcess;
        matches.append(v);
    };

    // Counters are direct evidence of throttling and should not wait for debounce.
    if (cpu.throttleCountersAvailable && cpu.deltaThrottleEvents > 0) {
        addCandidate(QStringLiteral("cpu.thermal-throttle"), VerdictSeverity::CriticalThrottle,
                     QStringLiteral("CPU & SoC"),
                     QStringLiteral("CPU thermal throttling observed"),
                     cpu.temperatureAvailable
                         ? QStringLiteral("%1°C package temperature; %2 new throttle events").arg(cpu.packageTempCelsius, 0, 'f', 1).arg(cpu.deltaThrottleEvents)
                         : QStringLiteral("%1 throttle events observed; package temperature is unavailable.").arg(cpu.deltaThrottleEvents),
                     QStringLiteral("Reduce sustained CPU load or choose a lower power profile."));
    }

    if (psiMem.memoryPsi.someAvailable
        && (psiMem.memoryPsi.someAvg10 >= 5.0
            || (psiMem.memoryPsi.fullAvailable && psiMem.memoryPsi.fullAvg10 >= 2.0))) {
        QString details = QStringLiteral("Memory pressure: %1% some pressure, %2 ms/s stalled")
                              .arg(psiMem.memoryPsi.someAvg10, 0, 'f', 1)
                              .arg(psiMem.memoryPsi.deltaStallMsPerSec, 0, 'f', 0);
        addCandidate(QStringLiteral("memory.pressure"), VerdictSeverity::Bottleneck,
                     QStringLiteral("Memory"),
                     QStringLiteral("Memory pressure is slowing the system"),
                     details,
                     QStringLiteral("Review memory use in System Monitor and close work you no longer need."));
    }

    if (psiMem.ioPsi.fullAvailable && psiMem.ioPsi.fullAvg10 >= 8.0) {
        double nvmeTemp = 0.0;
        bool nvmeTempAvailable = false;
        for (const DomainSensorReading &sensor : domains) {
            if (sensor.domain == PhysicalDomain::StorageNvme && sensor.available) {
                nvmeTemp = qMax(nvmeTemp, sensor.currentCelsius);
                nvmeTempAvailable = true;
            }
        }
        QString storageDetails = QStringLiteral("All-work I/O pressure is %1%; NVMe temperature is %2")
            .arg(psiMem.ioPsi.fullAvg10, 0, 'f', 1)
            .arg(nvmeTempAvailable ? QStringLiteral("%1°C").arg(nvmeTemp, 0, 'f', 1)
                                   : QStringLiteral("Unavailable"));
        if (psiMem.diskThroughputAvailable) {
            storageDetails += QStringLiteral("; disk throughput: %1 MiB/s read, %2 MiB/s write")
                .arg(psiMem.diskReadMibPerSec, 0, 'f', 1)
                .arg(psiMem.diskWriteMibPerSec, 0, 'f', 1);
        }
        addCandidate(QStringLiteral("storage.pressure"), VerdictSeverity::Bottleneck,
                     QStringLiteral("Storage"),
                     QStringLiteral("Storage pressure is blocking work"),
                     storageDetails,
                     QStringLiteral("Check for background indexing, updates, or other heavy disk activity."));
    }

    // Stable device ordering keeps equal-priority diagnoses independent of DRM enumeration.
    auto orderedGpus = gpus;
    std::sort(orderedGpus.begin(), orderedGpus.end(), [](const auto &a, const auto &b) { return a.identity < b.identity; });
    const auto gpuMatchesBegin = matches.size();
    const GpuSnapshot *busyGpu = nullptr;
    for (const auto &gpu : orderedGpus) {
        if (gpu.busyAvailable && (!busyGpu || gpu.busyPercent > busyGpu->busyPercent)) busyGpu = &gpu;
        const QString source = QStringLiteral("device %1; sensor %2 [%3]")
            .arg(gpu.identity.isEmpty() ? gpu.driverName : gpu.identity,
                 gpu.temperatureSensorLabel.isEmpty() ? QStringLiteral("unspecified") : gpu.temperatureSensorLabel,
                 gpu.temperatureSensorId.isEmpty() ? QStringLiteral("unspecified") : gpu.temperatureSensorId);
        if (gpu.busyAvailable && gpu.temperatureAvailable
            && gpu.busyPercent >= 92.0 && gpu.tempCelsius >= 75.0) {
            addCandidate(QStringLiteral("gpu.load-temperature"), VerdictSeverity::Bottleneck,
                         QStringLiteral("Graphics"),
                         QStringLiteral("GPU load and temperature are high"),
                         QStringLiteral("Measured GPU load: %1%; measured GPU temperature: %2°C (%3)")
                             .arg(gpu.busyPercent, 0, 'f', 0).arg(gpu.tempCelsius, 0, 'f', 1).arg(source),
                         QStringLiteral("Reduce graphics work or lower the frame-rate limit if display performance is affected."));
        } else if (gpu.temperatureAvailable && gpu.tempCelsius >= 88.0) {
            addCandidate(QStringLiteral("gpu.temperature"), VerdictSeverity::Elevated,
                         QStringLiteral("Graphics"),
                         QStringLiteral("GPU temperature is high"),
                         QStringLiteral("Measured GPU temperature: %1°C (%2). %3 Temperature alone does not establish a bottleneck.")
                             .arg(gpu.tempCelsius, 0, 'f', 1).arg(source)
                             .arg(gpu.busyAvailable ? QStringLiteral("Measured GPU load: %1%.").arg(gpu.busyPercent, 0, 'f', 0)
                                                    : QStringLiteral("GPU busy counter is unavailable.")),
                         QStringLiteral("Watch GPU temperature during graphics-intensive workloads."));
        }
    }
    std::stable_sort(matches.begin() + gpuMatchesBegin, matches.end(), [](const auto &a, const auto &b) {
        return static_cast<int>(a.severity) > static_cast<int>(b.severity);
    });

    if (cpu.temperatureAvailable && cpu.thermalLimitAvailable && cpu.thermalHeadroomCelsius <= 6.0) {
        addCandidate(QStringLiteral("cpu.thermal-headroom"), VerdictSeverity::Elevated,
                     QStringLiteral("CPU & SoC"),
                     QStringLiteral("CPU is close to its reported thermal limit"),
                     QStringLiteral("Measured temperature: %1°C; remaining headroom: %2°C. %3")
                         .arg(cpu.packageTempCelsius, 0, 'f', 1).arg(cpu.thermalHeadroomCelsius, 0, 'f', 1)
                         .arg(cpu.throttleCountersAvailable ? QStringLiteral("No new throttle event was observed.")
                                                            : QStringLiteral("Throttle counters are unavailable; throttling cannot be confirmed.")),
                     QStringLiteral("Watch the temperature during sustained work. A lower power profile can reduce heat."));
    }

    for (const DomainSensorReading &sensor : domains) {
        if (sensor.domain == PhysicalDomain::StorageNvme && sensor.available && sensor.currentCelsius >= 70.0) {
            addCandidate(QStringLiteral("storage.temperature"), VerdictSeverity::Elevated,
                         QStringLiteral("Storage temperature"),
                         QStringLiteral("NVMe temperature is elevated"),
                         QStringLiteral("Measured NVMe temperature: %1°C. %2")
                             .arg(sensor.currentCelsius, 0, 'f', 1)
                             .arg(psiMem.ioPsi.fullAvailable ? QStringLiteral("No high all-work I/O pressure was observed.")
                                                             : QStringLiteral("I/O pressure is unavailable; temperature alone does not establish an I/O bottleneck.")),
                         QStringLiteral("Keep an eye on storage temperature during long transfers."));
            break;
        }
    }

    if (power.activePowerProfile == QStringLiteral("power-saver")
        && ((cpuLoadValid && cpu.totalLoadPercent >= 65.0)
            || (busyGpu && busyGpu->busyPercent >= 75.0)
            || (psiMem.cpuPsi.someAvailable && psiMem.cpuPsi.someAvg10 >= 10.0))) {
        QString details = QStringLiteral("Active power profile is 'power-saver'");
        if (cpuLoadValid) {
            details += QStringLiteral("; measured CPU load is %1%").arg(cpu.totalLoadPercent, 0, 'f', 0);
        }
        if (busyGpu && busyGpu->busyPercent >= 75.0) {
            details += QStringLiteral("; measured GPU load is %1% (%2)").arg(busyGpu->busyPercent, 0, 'f', 0).arg(busyGpu->identity);
        }
        addCandidate(QStringLiteral("power.saver-workload"), VerdictSeverity::Elevated,
                     QStringLiteral("Power & Cooling"),
                     QStringLiteral("High workload while power-saver profile is active"),
                     details,
                     QStringLiteral("Switch to the 'balanced' or 'performance' power profile for higher responsiveness."));
    }

    if (cpuLoadValid && cpu.totalLoadPercent >= 75.0
        && power.packagePowerAvailable && !power.packagePowerEstimated
        && power.pl1LimitWatts > 5.0 && power.packageWatts >= power.pl1LimitWatts * 0.95) {
        addCandidate(QStringLiteral("cpu.sustained-power-limit"), VerdictSeverity::Elevated,
                     QStringLiteral("Power & Cooling"),
                     QStringLiteral("CPU is operating at its sustained power limit (PL1)"),
                     QStringLiteral("Measured package power: %1 W at %2% CPU load (PL1 limit: %3 W)")
                         .arg(power.packageWatts, 0, 'f', 1)
                         .arg(cpu.totalLoadPercent, 0, 'f', 0)
                         .arg(power.pl1LimitWatts, 0, 'f', 0),
                     QStringLiteral("Sustained performance is capped by the power envelope. Use 'performance' profile if higher limits are supported."));
    }

    int saturatedCoreId = -1;
    double maxCoreLoad = 0.0;
    for (const CoreTelemetry &core : cpu.cores) {
        if (core.loadAvailable && core.loadPercent >= 95.0 && core.loadPercent > maxCoreLoad) {
            maxCoreLoad = core.loadPercent;
            saturatedCoreId = core.coreId;
        }
    }
    if (saturatedCoreId >= 0 && cpu.cores.size() >= 2 && cpuLoadValid && cpu.totalLoadPercent < 75.0) {
        const bool cpuPsiElevated = psiMem.cpuPsi.someAvailable && psiMem.cpuPsi.someAvg10 >= 3.0;
        const bool processHeavy = cpuProcess.cpuPercent >= (80.0 / std::max(1, static_cast<int>(cpu.cores.size())));
        if (cpuPsiElevated || processHeavy || maxCoreLoad >= 98.0) {
            const VerdictSeverity sev = (cpuPsiElevated && psiMem.cpuPsi.someAvg10 >= 8.0)
                ? VerdictSeverity::Bottleneck : VerdictSeverity::Elevated;
            QString details = QStringLiteral("Core %1 is saturated at %2% load (total CPU load: %3%)")
                .arg(saturatedCoreId).arg(maxCoreLoad, 0, 'f', 0).arg(cpu.totalLoadPercent, 0, 'f', 0);
            if (cpuProcess.cpuPercent > 0.0 && !cpuProcess.name.isEmpty()) {
                details += QStringLiteral("; main load from '%1' (%2% CPU)").arg(cpuProcess.name).arg(cpuProcess.cpuPercent, 0, 'f', 0);
            }
            if (psiMem.cpuPsi.someAvailable) {
                details += QStringLiteral("; CPU pressure: %1%").arg(psiMem.cpuPsi.someAvg10, 0, 'f', 1);
            }
            addCandidate(QStringLiteral("cpu.single-core-saturation"), sev,
                         QStringLiteral("CPU & SoC"),
                         QStringLiteral("Single-core CPU saturation observed"),
                         details,
                         QStringLiteral("A single thread or process is bottlenecking work. Check the top culprit process in System Monitor."));
        }
    }

    if ((psiMem.cpuPsi.someAvailable && psiMem.cpuPsi.someAvg10 >= 10.0)
        || (cpu.temperatureAvailable && cpu.thermalLimitAvailable && cpu.thermalHeadroomCelsius < 20.0)) {
        QString details;
        if (cpuLoadValid) {
            details = QStringLiteral("Measured CPU load: %1%")
                          .arg(cpu.totalLoadPercent, 0, 'f', 0);
        }
        if (cpu.temperatureAvailable) {
            if (!details.isEmpty()) details += QStringLiteral("; ");
            details += QStringLiteral("Measured CPU temperature: %1°C").arg(cpu.packageTempCelsius, 0, 'f', 1);
        }
        if (psiMem.cpuPsi.someAvailable) {
            details += QStringLiteral("; CPU wait: %1%").arg(psiMem.cpuPsi.someAvg10, 0, 'f', 1);
        }
        if (details.isEmpty()) details = QStringLiteral("CPU pressure or thermal risk is elevated.");
        addCandidate(QStringLiteral("cpu.elevated"), VerdictSeverity::Elevated,
                     QStringLiteral("CPU & SoC"),
                     QStringLiteral("CPU pressure or thermal risk is elevated"),
                     details,
                     QStringLiteral("Check whether the current workload is expected."));
    }

    if (power.batteryStateAvailable && power.batteryPresent && !power.onAcPower
        && power.batteryPowerAvailable && power.batteryWatts >= 28.0) {
        QString details = QStringLiteral("Battery discharge rate is %1 W").arg(power.batteryWatts, 0, 'f', 1);
        if (power.batteryLevelAvailable) {
            details += QStringLiteral(" (%1% remaining)").arg(power.batteryPercent, 0, 'f', 0);
        }
        addCandidate(QStringLiteral("battery.high-discharge"), VerdictSeverity::Elevated,
                     QStringLiteral("Power & Cooling"),
                     QStringLiteral("High battery power draw"),
                     details,
                     QStringLiteral("Switch to the 'balanced' or 'power-saver' profile to extend battery runtime."));
    }

    if (!matches.isEmpty()) {
        verdict = matches.first();
        for (int i = 1; i < matches.size(); ++i) {
            const CausalVerdict &secondary = matches.at(i);
            if (secondary.primaryBottleneckDomain != verdict.primaryBottleneckDomain
                && !verdict.contributingSignals.contains(secondary.headline)) {
                verdict.contributingSignals.append(secondary.headline);
            }
        }
        return verdict;
    }

    if (!cpuLoadValid || !cpu.temperatureAvailable || !psiMem.responsivenessAvailable) {
        QStringList unavailable;
        if (!cpuLoadValid) unavailable.append(cpu.loadAvailable
            ? QStringLiteral("CPU load samples are warming up")
            : QStringLiteral("CPU load"));
        if (!cpu.temperatureAvailable) unavailable.append(QStringLiteral("CPU temperature"));
        if (!psiMem.responsivenessAvailable) unavailable.append(QStringLiteral("system pressure"));
        verdict.ruleId = QStringLiteral("system.incomplete");
        verdict.severity = VerdictSeverity::Unknown;
        verdict.primaryBottleneckDomain = QStringLiteral("Limited data");
        verdict.headline = QStringLiteral("Some system readings are unavailable");
        verdict.causalChain = QStringLiteral("Unavailable or incomplete readings: %1. Overall system status is not confirmed.")
            .arg(unavailable.join(QStringLiteral(" and ")));
        verdict.recommendation = QStringLiteral("Review the data quality details for missing sensors and interfaces.");
        verdict.evidence.append(verdict.causalChain);
        return verdict;
    }

    verdict.ruleId = QStringLiteral("system.nominal");
    verdict.severity = VerdictSeverity::Nominal;
    verdict.primaryBottleneckDomain = QStringLiteral("No current pressure");
    verdict.headline = QStringLiteral("No current system pressure detected");
    verdict.causalChain = QStringLiteral("No sustained pressure or thermal risk was observed in the available readings.");
    verdict.recommendation = QStringLiteral("No action is needed.");
    verdict.evidence.append(verdict.causalChain);
    return verdict;
}

CausalVerdict VerdictStabilizer::update(const CausalVerdict &candidate, qint64 monotonicMs)
{
    // Explicit rule identities must remain present continuously before replacing a verdict.
    // Legacy callers without IDs retain the previous severity-based contract below.
    if (!candidate.ruleId.isEmpty()) {
        const bool immediate = candidate.severity == VerdictSeverity::Unknown
            || candidate.severity == VerdictSeverity::CriticalThrottle;
        const QString identity = candidate.ruleId + QLatin1Char(':') + QString::number(static_cast<int>(candidate.severity));
        const bool sameCurrent = candidate.ruleId == m_current.ruleId && candidate.severity == m_current.severity;
        if (immediate || sameCurrent) {
            const qint64 since = sameCurrent ? m_current.observedSinceMs : monotonicMs;
            m_current = candidate;
            m_current.observedSinceMs = since;
            m_current.durationMs = qMax<qint64>(0, monotonicMs - since);
            m_pendingRuleId.clear();
            return m_current;
        }
        if (m_pendingRuleId != identity) {
            m_pendingRuleId = identity;
            m_pendingSinceMs = monotonicMs;
        }
        const bool warning = candidate.severity == VerdictSeverity::Elevated
            || candidate.severity == VerdictSeverity::Bottleneck;
        const qint64 requiredMs = warning ? 3000 : 5000;
        if (monotonicMs - m_pendingSinceMs >= requiredMs) {
            m_current = candidate;
            m_current.observedSinceMs = m_pendingSinceMs;
            m_current.durationMs = monotonicMs - m_pendingSinceMs;
            m_pendingRuleId.clear();
        }
        return m_current;
    }

    if (candidate.severity == VerdictSeverity::Unknown
        || candidate.severity == VerdictSeverity::CriticalThrottle) {
        m_current = candidate;
        m_pendingSeverity = candidate.severity;
        m_pendingSinceMs = 0;
        return m_current;
    }

    if (candidate.severity == m_current.severity) {
        m_current = candidate;
        m_pendingSeverity = candidate.severity;
        m_pendingSinceMs = 0;
        return m_current;
    }

    const auto awaitingConfirmation = [this]() {
        CausalVerdict result = m_current;
        if (result.severity == VerdictSeverity::Unknown) {
            result.headline = QStringLiteral("Confirming system readings");
            result.causalChain = QStringLiteral("Required measurements are available. Waiting for a consistent status before confirming the result.");
            result.recommendation = QStringLiteral("Keep Loofi Pulse open while readings stabilize.");
        }
        return result;
    };

    const bool currentIsUnwarned = (m_current.severity == VerdictSeverity::Nominal
                                    || m_current.severity == VerdictSeverity::Unknown);
    const bool candidateIsWarning = (candidate.severity == VerdictSeverity::Elevated
                                     || candidate.severity == VerdictSeverity::Bottleneck);
    const bool pendingIsWarning = (m_pendingSeverity == VerdictSeverity::Elevated
                                   || m_pendingSeverity == VerdictSeverity::Bottleneck);

    if (candidate.severity != m_pendingSeverity) {
        // Do not reset the 3s warning confirmation timer if oscillating between Elevated and Bottleneck
        // while transitioning out of Nominal/Unknown.
        if (!(currentIsUnwarned && candidateIsWarning && pendingIsWarning && m_pendingSinceMs > 0)) {
            m_pendingSeverity = candidate.severity;
            m_pendingSinceMs = monotonicMs;
            return awaitingConfirmation();
        }
        m_pendingSeverity = candidate.severity;
    }

    const bool confirmingWarning = candidate.severity != VerdictSeverity::Nominal
        && (m_current.severity == VerdictSeverity::Unknown
            || static_cast<int>(candidate.severity) > static_cast<int>(m_current.severity));
    const qint64 requiredMs = confirmingWarning ? 3000 : 5000;
    if (monotonicMs - m_pendingSinceMs >= requiredMs) {
        m_current = candidate;
        m_pendingSinceMs = 0;
    }
    return awaitingConfirmation();
}

VerdictStabilizer::VerdictStabilizer()
{
    m_current.severity = VerdictSeverity::Unknown;
    m_current.primaryBottleneckDomain = QStringLiteral("Limited data");
    m_current.headline = QStringLiteral("Collecting initial readings");
    m_current.causalChain = QStringLiteral("Waiting for stable measurements before confirming system status.");
    m_current.recommendation = QStringLiteral("Keep Loofi Pulse open while readings stabilize.");
}

} // namespace LoofiPulse
