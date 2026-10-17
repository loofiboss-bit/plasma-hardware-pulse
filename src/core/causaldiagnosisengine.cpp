#include "causaldiagnosisengine.h"

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
    CausalVerdict verdict;
    verdict.topProcess = cpuProcess;
    verdict.topMemoryProcess = memoryProcess;
    const bool cpuLoadValid = cpu.loadAvailable && cpu.loadWarmedUp;

    const auto setVerdict = [&verdict](VerdictSeverity severity, const QString &domain,
                                      const QString &headline, const QString &details,
                                      const QString &recommendation) {
        verdict.severity = severity;
        verdict.primaryBottleneckDomain = domain;
        verdict.headline = headline;
        verdict.causalChain = details;
        verdict.recommendation = recommendation;
    };

    // Counters are direct evidence of throttling and should not wait for debounce.
    if (cpu.throttleCountersAvailable && cpu.deltaThrottleEvents > 0) {
        setVerdict(VerdictSeverity::CriticalThrottle,
                   QStringLiteral("CPU & SoC"),
                   QStringLiteral("CPU thermal throttling observed"),
                   cpu.temperatureAvailable
                       ? QStringLiteral("%1°C package temperature; %2 new throttle events").arg(cpu.packageTempCelsius, 0, 'f', 1).arg(cpu.deltaThrottleEvents)
                       : QStringLiteral("%1 throttle events observed; package temperature is unavailable.").arg(cpu.deltaThrottleEvents),
                   QStringLiteral("Reduce sustained CPU load or choose a lower power profile."));
        return verdict;
    }

    if (psiMem.memoryPsi.someAvailable
        && (psiMem.memoryPsi.someAvg10 >= 5.0
            || (psiMem.memoryPsi.fullAvailable && psiMem.memoryPsi.fullAvg10 >= 2.0))) {
        QString details = QStringLiteral("Memory pressure: %1% some pressure, %2 ms/s stalled")
                              .arg(psiMem.memoryPsi.someAvg10, 0, 'f', 1)
                              .arg(psiMem.memoryPsi.deltaStallMsPerSec, 0, 'f', 0);
        if (!memoryProcess.name.isEmpty()) {
            details += QStringLiteral("; highest observed memory use is %1 (%2 MiB RSS)")
                           .arg(memoryProcess.name).arg(memoryProcess.rssMegabytes, 0, 'f', 0);
        }
        setVerdict(VerdictSeverity::Bottleneck,
                   QStringLiteral("Memory"),
                   QStringLiteral("Memory pressure is slowing the system"),
                   details,
                   QStringLiteral("Review memory use in System Monitor and close work you no longer need."));
        return verdict;
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
        setVerdict(VerdictSeverity::Bottleneck,
                   QStringLiteral("Storage"),
                   QStringLiteral("Storage pressure is blocking work"),
                   QStringLiteral("All-work I/O pressure is %1%; NVMe temperature is %2")
                       .arg(psiMem.ioPsi.fullAvg10, 0, 'f', 1)
                       .arg(nvmeTempAvailable ? QStringLiteral("%1°C").arg(nvmeTemp, 0, 'f', 1)
                                              : QStringLiteral("Unavailable")),
                   QStringLiteral("Check for background indexing, updates, or other heavy disk activity."));
        return verdict;
    }

    if (gpu.busyAvailable && gpu.temperatureAvailable
        && (gpu.tempCelsius >= 88.0 || (gpu.busyPercent >= 92.0 && gpu.tempCelsius >= 75.0))) {
        setVerdict(VerdictSeverity::Bottleneck,
                   QStringLiteral("Graphics"),
                   QStringLiteral("GPU load and temperature are high"),
                   QStringLiteral("Measured GPU load: %1%; measured GPU temperature: %2°C (%3)")
                       .arg(gpu.busyPercent, 0, 'f', 0).arg(gpu.tempCelsius, 0, 'f', 1).arg(gpu.driverName),
                   QStringLiteral("Reduce graphics work or lower the frame-rate limit if display performance is affected."));
        return verdict;
    }

    if (cpu.temperatureAvailable && cpu.thermalLimitAvailable && cpu.thermalHeadroomCelsius <= 6.0) {
        setVerdict(VerdictSeverity::Elevated,
                   QStringLiteral("CPU & SoC"),
                   QStringLiteral("CPU is close to its reported thermal limit"),
                   QStringLiteral("Measured temperature: %1°C; remaining headroom: %2°C. %3")
                       .arg(cpu.packageTempCelsius, 0, 'f', 1).arg(cpu.thermalHeadroomCelsius, 0, 'f', 1)
                       .arg(cpu.throttleCountersAvailable ? QStringLiteral("No new throttle event was observed.")
                                                         : QStringLiteral("Throttle counters are unavailable; throttling cannot be confirmed.")),
                   QStringLiteral("Watch the temperature during sustained work. A lower power profile can reduce heat."));
        return verdict;
    }

    for (const DomainSensorReading &sensor : domains) {
        if (sensor.domain == PhysicalDomain::StorageNvme && sensor.available && sensor.currentCelsius >= 70.0) {
            setVerdict(VerdictSeverity::Elevated,
                       QStringLiteral("Storage temperature"),
                       QStringLiteral("NVMe temperature is elevated"),
                       QStringLiteral("Measured NVMe temperature: %1°C. %2")
                           .arg(sensor.currentCelsius, 0, 'f', 1)
                           .arg(psiMem.ioPsi.fullAvailable ? QStringLiteral("No high all-work I/O pressure was observed.")
                                                         : QStringLiteral("I/O pressure is unavailable; temperature alone does not establish an I/O bottleneck.")),
                       QStringLiteral("Keep an eye on storage temperature during long transfers."));
            return verdict;
        }
    }

    if (gpu.busyAvailable && gpu.temperatureAvailable
        && cpuLoadValid && gpu.busyPercent >= 85.0 && cpu.totalLoadPercent < 70.0) {
        setVerdict(VerdictSeverity::Elevated,
                   QStringLiteral("Graphics"),
                   QStringLiteral("GPU load is high"),
                   QStringLiteral("Measured GPU load: %1% at %2°C (%3)")
                       .arg(gpu.busyPercent, 0, 'f', 0).arg(gpu.tempCelsius, 0, 'f', 1).arg(gpu.driverName),
                   QStringLiteral("Check whether graphics work is expected."));
        return verdict;
    }

    if ((cpuLoadValid && cpu.totalLoadPercent >= 70.0)
        || (cpu.temperatureAvailable && cpu.thermalVelocityCPerSec >= 0.8)
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
        if (!cpuProcess.name.isEmpty()) {
            if (!details.isEmpty()) details += QStringLiteral("; ");
            details += QStringLiteral("highest observed CPU use is %1 at %2%")
                           .arg(cpuProcess.name).arg(cpuProcess.cpuPercent, 0, 'f', 1);
        }
        if (details.isEmpty()) details = QStringLiteral("A CPU or thermal reading is elevated.");
        setVerdict(VerdictSeverity::Elevated,
                   QStringLiteral("CPU & SoC"),
                   QStringLiteral("CPU load or temperature is elevated"),
                   details,
                   QStringLiteral("Check whether the current workload is expected."));
        return verdict;
    }

    if (!cpuLoadValid || !cpu.temperatureAvailable || !psiMem.responsivenessAvailable) {
        QStringList unavailable;
        if (!cpuLoadValid) unavailable.append(cpu.loadAvailable
            ? QStringLiteral("CPU load samples are warming up")
            : QStringLiteral("CPU load"));
        if (!cpu.temperatureAvailable) unavailable.append(QStringLiteral("CPU temperature"));
        if (!psiMem.responsivenessAvailable) unavailable.append(QStringLiteral("system pressure"));
        setVerdict(VerdictSeverity::Unknown,
                   QStringLiteral("Limited data"),
                   QStringLiteral("Some system readings are unavailable"),
                   QStringLiteral("Unavailable or incomplete readings: %1. Overall system status is not confirmed.")
                       .arg(unavailable.join(QStringLiteral(" and "))),
                   QStringLiteral("Review the data quality details for missing sensors and interfaces."));
        return verdict;
    }

    setVerdict(VerdictSeverity::Nominal,
               QStringLiteral("No current pressure"),
               QStringLiteral("No current system pressure detected"),
               QStringLiteral("CPU load and system pressure are within normal ranges."),
               QStringLiteral("No action is needed."));
    Q_UNUSED(power);
    return verdict;
}

CausalVerdict VerdictStabilizer::update(const CausalVerdict &candidate, qint64 monotonicMs)
{
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

    if (candidate.severity != m_pendingSeverity) {
        m_pendingSeverity = candidate.severity;
        m_pendingSinceMs = monotonicMs;
        return awaitingConfirmation();
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
