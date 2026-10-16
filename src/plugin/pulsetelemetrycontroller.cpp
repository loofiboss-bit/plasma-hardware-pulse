#include "pulsetelemetrycontroller.h"

#include <KNotification>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>

#include <algorithm>

namespace LoofiPulse
{

PulseTelemetryController::PulseTelemetryController(QObject *parent)
    : QObject(parent)
    , m_recorder(300)
{
    m_elapsed.start();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &PulseTelemetryController::refreshNow);

    // Take two immediate initial samples so delta metrics (CPU load, PSI rate) are populated immediately
    refreshNow();
    refreshNow();
    m_timer.start();
}

void PulseTelemetryController::setPollIntervalMs(int intervalMs)
{
    const int clamped = std::clamp(intervalMs, 250, 10000);
    if (m_timer.interval() == clamped) {
        return;
    }
    m_timer.setInterval(clamped);
    Q_EMIT pollIntervalChanged();
}

void PulseTelemetryController::setNotificationsEnabled(bool enabled)
{
    if (m_notificationsEnabled == enabled) {
        return;
    }
    m_notificationsEnabled = enabled;
    Q_EMIT notificationsEnabledChanged();
}

void PulseTelemetryController::setRecordingPaused(bool paused)
{
    if (m_recordingPaused == paused) {
        return;
    }
    m_recordingPaused = paused;
    setLastActionStatus(paused ? QStringLiteral("Flight Recorder paused") : QStringLiteral("Flight Recorder resumed"));
    Q_EMIT recordingPausedChanged();
}

void PulseTelemetryController::setLastActionStatus(const QString &status)
{
    if (m_lastActionStatus == status) {
        return;
    }
    m_lastActionStatus = status;
    Q_EMIT lastActionStatusChanged();
}

QString PulseTelemetryController::batteryTimeRemainingText() const
{
    if (!m_power.batteryPresent) {
        return QStringLiteral("AC Desktop");
    }
    const int mins = m_power.batteryTimeRemainingMinutes;
    if (mins > 0) {
        const int h = mins / 60;
        const int m = mins % 60;
        if (h > 0) {
            return QStringLiteral("~%1h %2m").arg(h).arg(m, 2, 10, QLatin1Char('0'));
        }
        return QStringLiteral("~%1m").arg(m);
    }
    if (m_power.onAcPower) {
        return QStringLiteral("On AC (%1%)").arg(qRound(m_power.batteryPercent));
    }
    return m_power.batteryStatus.isEmpty() ? QStringLiteral("N/A") : m_power.batteryStatus;
}

void PulseTelemetryController::setSelectedHistoryIndex(int index)
{
    const int clamped = (index < 0 || index >= m_recorder.count()) ? -1 : index;
    if (m_selectedHistoryIndex == clamped) {
        return;
    }
    m_selectedHistoryIndex = clamped;
    Q_EMIT selectedHistoryChanged();
}

QVariantMap PulseTelemetryController::sampleToVariantMap(const FlightSample &s, int index)
{
    QVariantMap m;
    m.insert(QStringLiteral("index"), index);
    m.insert(QStringLiteral("timestampMs"), s.timestampMs);
    m.insert(QStringLiteral("timeString"), QDateTime::fromMSecsSinceEpoch(s.timestampMs).toString(QStringLiteral("HH:mm:ss")));
    m.insert(QStringLiteral("cpuTemp"), s.cpuTempCelsius);
    m.insert(QStringLiteral("nvmeTemp"), s.nvmeTempCelsius);
    m.insert(QStringLiteral("wifiTemp"), s.wifiTempCelsius);
    m.insert(QStringLiteral("cpuLoad"), s.cpuLoadPercent);
    m.insert(QStringLiteral("cpuFreqMhz"), s.cpuFreqMhz);
    m.insert(QStringLiteral("gpuBusy"), s.gpuBusyPercent);
    m.insert(QStringLiteral("gpuTemp"), s.gpuTempCelsius);
    m.insert(QStringLiteral("psiCpuSome10"), s.psiCpuSome10);
    m.insert(QStringLiteral("psiMemSome10"), s.psiMemSome10);
    m.insert(QStringLiteral("psiIoFull10"), s.psiIoFull10);
    m.insert(QStringLiteral("responsivenessScore"), s.responsivenessScore);
    m.insert(QStringLiteral("systemWatts"), s.systemWatts);
    m.insert(QStringLiteral("throttled"), s.throttled);
    m.insert(QStringLiteral("severity"), static_cast<int>(s.severity));
    m.insert(QStringLiteral("verdictHeadline"), s.verdictHeadline);
    m.insert(QStringLiteral("culpritName"), s.culpritName.isEmpty() ? QStringLiteral("No single process") : s.culpritName);
    m.insert(QStringLiteral("culpritPid"), s.culpritPid);
    m.insert(QStringLiteral("culpritCpuPercent"), s.culpritCpuPercent);
    m.insert(QStringLiteral("culpritRssMb"), s.culpritRssMb);
    return m;
}

QVariantMap PulseTelemetryController::selectedHistoryDetail() const
{
    const int idx = (m_selectedHistoryIndex >= 0 && m_selectedHistoryIndex < m_recorder.count())
        ? m_selectedHistoryIndex
        : (m_recorder.count() - 1);
    if (idx < 0) {
        return {};
    }
    return sampleToVariantMap(m_recorder.sampleAt(idx), idx);
}

void PulseTelemetryController::refreshNow()
{
    const qint64 elapsedMs = std::max<qint64>(100, m_elapsed.restart());

    m_domains = m_sysfsReader.sampleHwmonDomains(elapsedMs);
    m_cpu = m_cpuSampler.sampleCpu(m_domains);
    m_gpu = m_cpuSampler.sampleGpu(m_domains);
    m_psiMem = m_psiSampler.sample(elapsedMs);
    m_power = m_powerSampler.sample(elapsedMs, m_cpu.totalLoadPercent, m_cpu.avgFreqMhz, m_cpu.maxFreqMhz);
    m_processRadar = m_cpuSampler.sampleProcessRadar(5);
    m_verdict = CausalDiagnosisEngine::evaluate(
        m_cpu,
        m_gpu,
        m_psiMem,
        m_power,
        m_domains,
        m_processRadar.topCpuProcess,
        m_processRadar.topMemoryProcess);

    m_nvmeTemp = 0.0;
    m_wifiTemp = 0.0;
    m_domainSensorsVariant.clear();

    for (const DomainSensorReading &d : std::as_const(m_domains)) {
        if (d.domain == PhysicalDomain::StorageNvme && d.currentCelsius > m_nvmeTemp) {
            m_nvmeTemp = d.currentCelsius;
        } else if (d.domain == PhysicalDomain::NetworkWireless && d.currentCelsius > m_wifiTemp) {
            m_wifiTemp = d.currentCelsius;
        }

        QVariantMap dm;
        dm.insert(QStringLiteral("id"), d.id);
        dm.insert(QStringLiteral("chipName"), d.chipName);
        dm.insert(QStringLiteral("label"), d.label);
        dm.insert(QStringLiteral("domainId"), static_cast<int>(d.domain));
        dm.insert(QStringLiteral("domainName"), SysfsReader::domainDisplayName(d.domain));
        dm.insert(QStringLiteral("currentCelsius"), d.currentCelsius);
        dm.insert(QStringLiteral("maxCelsius"), d.maxCelsius);
        dm.insert(QStringLiteral("headroomCelsius"), d.headroomCelsius);
        dm.insert(QStringLiteral("velocityCelsiusPerSec"), d.velocityCelsiusPerSec);
        m_domainSensorsVariant.append(dm);
    }

    m_coreItemsVariant.clear();
    for (const CoreTelemetry &c : std::as_const(m_cpu.cores)) {
        QVariantMap cm;
        cm.insert(QStringLiteral("coreId"), c.coreId);
        cm.insert(QStringLiteral("loadPercent"), c.loadPercent);
        cm.insert(QStringLiteral("currentFreqMhz"), c.currentFreqMhz);
        cm.insert(QStringLiteral("maxFreqMhz"), c.maxFreqMhz);
        cm.insert(QStringLiteral("tempCelsius"), c.tempCelsius);
        cm.insert(QStringLiteral("throttleCount"), c.throttleCount);
        m_coreItemsVariant.append(cm);
    }

    m_topProcessesVariant.clear();
    for (const CulpritProcess &p : std::as_const(m_processRadar.topProcesses)) {
        QVariantMap pm;
        pm.insert(QStringLiteral("pid"), p.pid);
        pm.insert(QStringLiteral("name"), p.name);
        pm.insert(QStringLiteral("cpuPercent"), p.cpuPercent);
        pm.insert(QStringLiteral("rssMegabytes"), p.rssMegabytes);
        m_topProcessesVariant.append(pm);
    }

    if (!m_recordingPaused || m_recorder.count() == 0) {
        const CulpritProcess &primaryCulprit = (m_verdict.primaryBottleneckDomain.contains(QStringLiteral("Memory")))
            ? m_verdict.topMemoryProcess
            : m_verdict.topProcess;

        FlightSample sample;
        sample.timestampMs = QDateTime::currentMSecsSinceEpoch();
        sample.cpuTempCelsius = m_cpu.packageTempCelsius;
        sample.nvmeTempCelsius = m_nvmeTemp;
        sample.wifiTempCelsius = m_wifiTemp;
        sample.cpuLoadPercent = m_cpu.totalLoadPercent;
        sample.cpuFreqMhz = m_cpu.avgFreqMhz;
        sample.gpuBusyPercent = m_gpu.busyPercent;
        sample.gpuTempCelsius = m_gpu.tempCelsius;
        sample.psiCpuSome10 = m_psiMem.cpuPsi.someAvg10;
        sample.psiMemSome10 = m_psiMem.memoryPsi.someAvg10;
        sample.psiIoFull10 = m_psiMem.ioPsi.fullAvg10;
        sample.responsivenessScore = m_psiMem.responsivenessScore;
        sample.systemWatts = (m_power.batteryWatts > 0.5) ? m_power.batteryWatts : m_power.packageWatts;
        sample.throttled = (m_cpu.deltaThrottleEvents > 0);
        sample.severity = m_verdict.severity;
        sample.verdictHeadline = m_verdict.headline;
        sample.culpritName = primaryCulprit.name;
        sample.culpritPid = primaryCulprit.pid;
        sample.culpritCpuPercent = primaryCulprit.cpuPercent;
        sample.culpritRssMb = primaryCulprit.rssMegabytes;

        m_recorder.appendSample(sample);

        if (m_historySamplesVariant.size() >= m_recorder.capacity()) {
            m_historySamplesVariant.removeFirst();
        }
        m_historySamplesVariant.append(sampleToVariantMap(sample, m_recorder.count() - 1));
    }

    maybeEmitAlertNotifications();

    Q_EMIT telemetryUpdated();
    Q_EMIT selectedHistoryChanged();
}

void PulseTelemetryController::maybeEmitAlertNotifications()
{
    if (!m_notificationsEnabled) {
        return;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastAlertTimestampMs < 60000) {
        return; // 60s cooldown between alert popups
    }

    if (m_verdict.severity == VerdictSeverity::CriticalThrottle) {
        m_lastAlertTimestampMs = nowMs;
        auto *notif = new KNotification(QStringLiteral("thermalThrottle"), KNotification::CloseOnTimeout, this);
        notif->setComponentName(QStringLiteral("plasma_applet_com.github.loofi.hardwarepulse"));
        notif->setTitle(m_verdict.headline);
        notif->setText(m_verdict.causalChain);
        notif->sendEvent();
    } else if (m_verdict.severity == VerdictSeverity::Bottleneck && m_psiMem.responsivenessScore < 85.0) {
        m_lastAlertTimestampMs = nowMs;
        auto *notif = new KNotification(QStringLiteral("psiBottleneck"), KNotification::CloseOnTimeout, this);
        notif->setComponentName(QStringLiteral("plasma_applet_com.github.loofi.hardwarepulse"));
        notif->setTitle(m_verdict.headline);
        notif->setText(m_verdict.causalChain);
        notif->sendEvent();
    }
}

bool PulseTelemetryController::setPowerProfile(const QString &profile)
{
    const bool ok = PowerFlowSampler::setSystemPowerProfile(profile);
    if (ok) {
        m_power.activePowerProfile = profile;
        setLastActionStatus(QStringLiteral("Power profile switched to '%1'").arg(profile));
        Q_EMIT telemetryUpdated();
    }
    return ok;
}

void PulseTelemetryController::launchStudioApp()
{
    QString exe = QStandardPaths::findExecutable(QStringLiteral("loofi-pulse"));
    if (exe.isEmpty()) {
        const QString sibling = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("loofi-pulse"));
        if (QFileInfo::exists(sibling)) {
            exe = sibling;
        }
    }
    if (!exe.isEmpty()) {
        QProcess::startDetached(exe, {});
    }
}

int PulseTelemetryController::jumpToPeakThermalSpike()
{
    const int idx = m_recorder.peakTemperatureIndex();
    if (idx >= 0) {
        setSelectedHistoryIndex(idx);
    }
    return idx;
}

int PulseTelemetryController::jumpToWorstStallSpike()
{
    const int idx = m_recorder.worstResponsivenessIndex();
    if (idx >= 0) {
        setSelectedHistoryIndex(idx);
    }
    return idx;
}

bool PulseTelemetryController::terminateProcess(int pid, const QString &name)
{
    const bool ok = CpuSocSampler::terminateProcess(pid);
    const QString label = name.isEmpty() ? QStringLiteral("PID %1").arg(pid) : QStringLiteral("%1 (PID %2)").arg(name).arg(pid);
    if (ok) {
        setLastActionStatus(QStringLiteral("Sent SIGTERM to %1").arg(label));
        QTimer::singleShot(350, this, &PulseTelemetryController::refreshNow);
    } else {
        setLastActionStatus(QStringLiteral("Failed to terminate %1 (permission denied or exited)").arg(label));
    }
    return ok;
}

QString PulseTelemetryController::copyIncidentReportToClipboard()
{
    const QString md = m_recorder.exportSummaryMarkdown();
    if (QClipboard *cb = QGuiApplication::clipboard()) {
        cb->setText(md);
        setLastActionStatus(QStringLiteral("Copied Causal Incident Report (Markdown) to clipboard"));
    }
    return md;
}

QString PulseTelemetryController::exportIncidentReportToFile()
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString reportsDir = QDir(baseDir).filePath(QStringLiteral("reports"));
    QDir().mkpath(reportsDir);

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString mdPath = QDir(reportsDir).filePath(QStringLiteral("loofi-pulse-report-%1.md").arg(stamp));
    const QString jsonPath = QDir(reportsDir).filePath(QStringLiteral("loofi-pulse-samples-%1.json").arg(stamp));

    QFile mdFile(mdPath);
    if (mdFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        mdFile.write(m_recorder.exportSummaryMarkdown().toUtf8());
        mdFile.close();
    }

    QFile jsonFile(jsonPath);
    if (jsonFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        jsonFile.write(m_recorder.exportSamplesJson().toUtf8());
        jsonFile.close();
    }

    setLastActionStatus(QStringLiteral("Saved report & JSON to %1").arg(mdPath));
    return mdPath;
}

} // namespace LoofiPulse
