#include "pulsetelemetrycontroller.h"
#include "collectorprotocol.h"
#include <QCoreApplication>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QPointer>
#include <QProcess>
#include <QRunnable>
#include <QStandardPaths>
#include <QUrl>

namespace LoofiPulse {
namespace {
QByteArray json(const QVariantMap &map) { return QJsonDocument(QJsonObject::fromVariantMap(map)).toJson(QJsonDocument::Compact); }
QVariantMap object(const QByteArray &bytes) { return QJsonDocument::fromJson(bytes).object().toVariantMap(); }
QVariantList incidentRows(const FlightRecorder &recorder) {
    auto result = recorder.incidentEpisodes();
    const auto markers = recorder.markers();
    for (int i = std::max(0, int(markers.size()) - 8); i < markers.size(); ++i) {
        const auto marker = markers.at(i).toMap();
        const auto id = marker.value(QStringLiteral("sampleId"));
        result.append(QVariantMap{{QStringLiteral("incidentId"), marker.value(QStringLiteral("markerId"))},
            {QStringLiteral("sampleId"), id}, {QStringLiteral("startSampleId"), id}, {QStringLiteral("endSampleId"), id},
            {QStringLiteral("manual"), true}, {QStringLiteral("active"), false}, {QStringLiteral("severity"), 0},
            {QStringLiteral("durationSeconds"), 0}, {QStringLiteral("headline"), marker.value(QStringLiteral("label"))},
            {QStringLiteral("timeString"), QDateTime::fromMSecsSinceEpoch(marker.value(QStringLiteral("timestampMs")).toLongLong()).toString(QStringLiteral("HH:mm:ss"))}});
    }
    return result;
}
QString localPath(const QString &path) { const QUrl url(path); return url.isLocalFile() ? url.toLocalFile() : path; }
}

PulseTelemetryController::~PulseTelemetryController()
{
    if (m_remoteClient) {
        QDBusMessage request = QDBusMessage::createMethodCall(QString::fromLatin1(CollectorServiceName),
            QString::fromLatin1(CollectorObjectPath), QString::fromLatin1(CollectorInterface), QStringLiteral("Unsubscribe"));
        request << m_clientId;
        QDBusConnection::sessionBus().asyncCall(request);
    }
    m_filePool.waitForDone();
}

void PulseTelemetryController::callCollector(const QString &method, const QVariantList &arguments,
                                             std::function<void(const QByteArray &)> completion)
{
    QDBusMessage request = QDBusMessage::createMethodCall(QString::fromLatin1(CollectorServiceName),
        QString::fromLatin1(CollectorObjectPath), QString::fromLatin1(CollectorInterface), method);
    request.setArguments(arguments);
    const auto generation = m_connectionGeneration;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 10000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, completion, method, generation](QDBusPendingCallWatcher *done) {
        QDBusPendingReply<QByteArray> reply = *done;
        done->deleteLater();
        if ((method == QStringLiteral("Subscribe") || method == QStringLiteral("History")) && generation != m_connectionGeneration) return;
        if (reply.isError()) {
            if (method == QStringLiteral("Subscribe") || method == QStringLiteral("History")
                || reply.error().type() == QDBusError::ServiceUnknown || reply.error().type() == QDBusError::Disconnected) {
                m_connecting = false; m_syncing = false; m_pendingSamples.clear();
                m_connectionState = QStringLiteral("Disconnected — %1").arg(reply.error().message());
            }
            if (method == QStringLiteral("Subscribe")) {
                // Build-tree execution uses the sibling helper; installed execution uses D-Bus activation.
                const QString sibling = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("loofi-pulse-service"));
                if ((!m_lastLaunchAttempt.isValid() || m_lastLaunchAttempt.elapsed() > 30000) && QFileInfo(sibling).isExecutable()) {
                    m_lastLaunchAttempt.start();
                    QProcess::startDetached(sibling, {});
                }
            } else setActionResult(false, QStringLiteral("Collector action failed: %1").arg(reply.error().message()));
            Q_EMIT telemetryUpdated();
            return;
        }
        if (completion) completion(reply.value());
    });
}

void PulseTelemetryController::connectCollector()
{
    if (!m_remoteClient || m_connecting) return;
    subscribeCollector();
}

void PulseTelemetryController::subscribeCollector()
{
    if (!m_remoteClient) return;
    m_connecting = true;
    callCollector(QStringLiteral("Subscribe"), {m_clientId, pollIntervalMs(), historyRetentionSeconds(), notificationsEnabled()},
        [this](const QByteArray &data) {
            m_connecting = false;
            const auto packet = object(data);
            const QString id = packet.value(QStringLiteral("captureId")).toString();
            const bool newCapture = m_remoteCaptureId != id;
            if (newCapture) {
                ++m_connectionGeneration; m_pendingSamples.clear();
                m_remoteCaptureId = id;
                m_liveRecorder.clear();
                if (!m_sessionMode) { m_recorder.clear(); m_historySamplesVariant.clear(); m_historyPlotSamples.clear(); m_selectedHistorySampleId = 0; m_selectedHistoryIndex = -1; m_selectedHistorySnapshot.clear(); }
            }
            if (newCapture || !m_receivedAt.isValid() || m_connectionState != QStringLiteral("Live")) {
                m_syncing = true;
                fetchCollectorHistory(newCapture || !m_liveRecorder.count() ? 0 : m_liveRecorder.samples().last().sampleId);
            }
            receiveCollectorFrame(data);
        });
}

void PulseTelemetryController::fetchCollectorHistory(qulonglong afterId)
{
    callCollector(QStringLiteral("History"), {QVariant::fromValue(afterId), 64}, [this, afterId](const QByteArray &bytes) {
        const auto packet = object(bytes);
        if (packet.value(QStringLiteral("captureId")).toString() != m_remoteCaptureId) {
            m_syncing = false; subscribeCollector(); return;
        }
        const auto samples = packet.value(QStringLiteral("samples")).toList();
        qulonglong latest = afterId;
        for (const auto &value : samples) {
            const auto map = value.toMap();
            latest = map.value(QStringLiteral("sampleId")).toULongLong();
            appendRemoteSample(map);
        }
        if (packet.value(QStringLiteral("more")).toBool() && latest > afterId) fetchCollectorHistory(latest);
        else {
            m_syncing = false;
            // Packets arriving during history transfer are replayed in order after the snapshot.
            for (const auto &sample : std::as_const(m_pendingSamples)) appendRemoteSample(sample.toMap());
            m_pendingSamples.clear();
            if (!m_sessionMode) rebuildHistoryView();
            Q_EMIT telemetryUpdated();
        }
    });
}

void PulseTelemetryController::receiveCollectorFrame(const QByteArray &data)
{
    const QVariantMap packet = object(data);
    if (packet.isEmpty()) return;
    const QString capture = packet.value(QStringLiteral("captureId")).toString();
    if (!m_remoteCaptureId.isEmpty() && capture != m_remoteCaptureId) {
        ++m_connectionGeneration; m_pendingSamples.clear(); m_syncing = false;
        m_remoteCaptureId.clear();
        m_connectionState = QStringLiteral("Collector restarted — recording gap");
        subscribeCollector(); return;
    }
    const auto values = packet.value(QStringLiteral("live")).toMap();
    for (auto it = values.cbegin(); it != values.cend(); ++it) m_remoteValues[it.key()] = it.value();
    m_receivedAt.restart();
    m_connectionState = m_recordingPaused ? QStringLiteral("Paused") : QStringLiteral("Live");
    const auto sample = packet.value(QStringLiteral("sample")).toMap();
    if (!sample.isEmpty() && !m_recordingPaused) {
        if (m_syncing) {
            m_pendingSamples.append(sample);
            if (m_pendingSamples.size() > 7201) m_pendingSamples.removeFirst();
        } else appendRemoteSample(sample);
    }
    m_remoteValues[QStringLiteral("recordedIncidents")] = packet.value(QStringLiteral("incidents"));
    if (!m_sessionMode) m_incidentHistoryVariant = packet.value(QStringLiteral("incidents")).toList();
    Q_EMIT telemetryUpdated();
    Q_EMIT selectedHistoryChanged();
}

void PulseTelemetryController::appendRemoteSample(const QVariantMap &map)
{
    FlightSample sample;
    if (!SessionStore::sampleFromMap(map, &sample)) return;
    if (sample.captureId != m_remoteCaptureId) return;
    if (m_liveRecorder.count() && sample.sampleId <= m_liveRecorder.samples().last().sampleId) return;
    if (m_gapPending) { sample.metricQuality[QStringLiteral("gapBefore")] = true; m_gapPending = false; }
    m_liveRecorder.appendSample(sample);
    if (m_sessionMode) return;
    m_recorder.appendSample(sample);
    m_historySamplesVariant.append(sampleForView(sample, m_recorder.count() - 1));
    while (m_historySamplesVariant.size() > m_recorder.count()) m_historySamplesVariant.removeFirst();
    m_incidentHistoryVariant = m_recorder.incidentEpisodes();
}

QByteArray PulseTelemetryController::collectorPacket(bool includeHistory) const
{
    QVariantMap live;
    const QSet<QByteArray> excluded{"historySamples", "incidentHistory", "actualHistorySeconds", "investigationSummary",
        "sessionMode", "sessionPath", "unsavedInvestigation", "backgroundMonitoring", "startAtLogin", "actionPending"};
    const auto *meta = metaObject();
    for (int i = QObject::staticMetaObject.propertyCount(); i < meta->propertyCount(); ++i) {
        const QMetaProperty property = meta->property(i);
        if (QByteArray(property.name()) == QByteArrayLiteral("allProcesses") && !includeHistory && m_lastPacketProcessMs == m_processRadar.observedAtMs) continue;
        if (!excluded.contains(property.name()) && property.notifySignal().name() == QByteArrayLiteral("telemetryUpdated"))
            live[QString::fromLatin1(property.name())] = property.read(this);
    }
    m_lastPacketProcessMs = m_processRadar.observedAtMs;
    live[QStringLiteral("effectivePollIntervalMs")] = pollIntervalMs();
    live[QStringLiteral("effectiveRetentionSeconds")] = historyRetentionSeconds();
    QVariantMap packet{{QStringLiteral("captureId"), m_captureId}, {QStringLiteral("live"), live},
        {QStringLiteral("incidents"), incidentRows(m_recorder)}};
    if (m_recorder.count()) packet[QStringLiteral("sample")] = SessionStore::sampleToMap(m_recorder.samples().last());
    if (includeHistory) packet[QStringLiteral("count")] = m_recorder.count();
    return json(packet);
}

QByteArray PulseTelemetryController::collectorHistory(qulonglong afterId, int limit) const
{
    QVariantList samples;
    bool more = false;
    for (const auto &s : m_recorder.samples()) {
        if (s.sampleId <= afterId) continue;
        if (samples.size() >= std::clamp(limit, 1, 64)) { more = true; break; }
        samples.append(SessionStore::sampleToMap(s));
    }
    return json({{QStringLiteral("captureId"), m_captureId}, {QStringLiteral("samples"), samples}, {QStringLiteral("more"), more}});
}

QByteArray PulseTelemetryController::collectorIncident(const QString &id) const
{
    QString error;
    if (m_markedInvestigations.contains(id)) return SessionStore::encode(m_markedInvestigations.value(id), true, &error);
    auto snapshot = m_recorder.freezeIncident(id);
    if (snapshot.samples.isEmpty()) for (const auto &value : m_recorder.markers()) {
        const auto marker = value.toMap();
        if (marker.value(QStringLiteral("markerId")).toString() == id) {
            const auto sampleId = marker.value(QStringLiteral("sampleId")).toULongLong();
            snapshot = m_recorder.freezeRange(sampleId, sampleId);
            snapshot.metadata[QStringLiteral("markerId")] = id;
            break;
        }
    }
    return SessionStore::encode(snapshot, true, &error);
}


void PulseTelemetryController::refreshProcesses()
{
    if (m_remoteClient) return;
    const qint64 now = m_uptime.elapsed();
    m_processSampler.sampleCpu({}, m_lastProcessScanBoottimeMs ? std::max<qint64>(1, now - m_lastProcessScanBoottimeMs) : 1000);
    m_processRadar = m_processSampler.sampleProcessRadar(8);
    m_lastProcessScanBoottimeMs = std::max<qint64>(1, now);
    m_allProcesses.clear(); m_topProcessesVariant.clear();
    for (const auto &p : m_processRadar.allProcesses) m_allProcesses.append(processToMap(p));
    for (const auto &p : m_processRadar.topProcesses) m_topProcessesVariant.append(processToMap(p));
    if (!m_recordingPaused) for (const auto &p : m_processRadar.historicalProcesses) m_processObservations.append(processToMap(p));
    while (m_processObservations.size() > 144) m_processObservations.removeFirst();
    Q_EMIT telemetryUpdated();
}

QVariantMap PulseTelemetryController::processToMap(const CulpritProcess &p) const
{
    return {{QStringLiteral("pid"), p.pid}, {QStringLiteral("name"), p.name}, {QStringLiteral("owner"), p.ownerName},
        {QStringLiteral("ownerUid"), p.ownerUid}, {QStringLiteral("cpuAvailable"), p.cpuAvailable},
        {QStringLiteral("startTimeTicks"), QVariant::fromValue(p.startTimeTicks)}, {QStringLiteral("cpuPercent"), p.cpuPercent},
        {QStringLiteral("rssMegabytes"), p.rssMegabytes}, {QStringLiteral("state"), p.state},
        {QStringLiteral("inDiskWait"), p.inDiskWait}, {QStringLiteral("observedAtMs"), p.observedAtMs}};
}

QVariantList PulseTelemetryController::allProcesses() const { return remoteValue<QVariantList>("allProcesses", m_allProcesses); }
QVariantList PulseTelemetryController::storageDevices() const
{
    if (m_remoteClient) return remoteCollection("storageDevices");
    QVariantList result;
    for (const auto &d : m_psiMem.disks) result.append(QVariantMap{{QStringLiteral("id"), d.identity}, {QStringLiteral("name"), d.name},
        {QStringLiteral("physical"), d.isPhysical}, {QStringLiteral("partition"), d.isPartition}, {QStringLiteral("included"), d.included},
        {QStringLiteral("available"), d.throughputAvailable}, {QStringLiteral("readMibPerSec"), d.readMibPerSec},
        {QStringLiteral("writeMibPerSec"), d.writeMibPerSec}, {QStringLiteral("iops"), d.readIops + d.writeIops},
        {QStringLiteral("requestMs"), d.averageRequestMs}, {QStringLiteral("requestTimeAvailable"), d.requestTimeAvailable}});
    return result;
}
QVariantList PulseTelemetryController::gpuDevices() const
{
    if (m_remoteClient) return remoteCollection("gpuDevices");
    QVariantList result;
    for (const auto &g : m_gpus) result.append(QVariantMap{{QStringLiteral("id"), g.identity}, {QStringLiteral("name"), g.name},
        {QStringLiteral("available"), g.available}, {QStringLiteral("busyAvailable"), g.busyAvailable},
        {QStringLiteral("temperatureAvailable"), g.temperatureAvailable}, {QStringLiteral("busyPercent"), g.busyPercent},
        {QStringLiteral("tempCelsius"), g.tempCelsius}, {QStringLiteral("detail"), g.unavailableReason},
        {QStringLiteral("powerAvailable"), g.powerAvailable}, {QStringLiteral("powerWatts"), g.powerWatts},
        {QStringLiteral("boostFreqMhz"), g.boostFreqMhz}, {QStringLiteral("currentFreqMhz"), g.currentFreqMhz}, {QStringLiteral("frequencyAvailable"), g.frequencyAvailable},
        {QStringLiteral("driverName"), g.driverName}, {QStringLiteral("vramAvailable"), g.vramAvailable},
        {QStringLiteral("vramUsedMegabytes"), g.vramUsedMegabytes}, {QStringLiteral("vramTotalMegabytes"), g.vramTotalMegabytes}});
    return result;
}
QVariantList PulseTelemetryController::batteries() const
{
    if (m_remoteClient) return remoteCollection("batteries");
    QVariantList result;
    for (const auto &b : m_power.batteries) result.append(QVariantMap{{QStringLiteral("id"), b.identity}, {QStringLiteral("name"), b.name},
        {QStringLiteral("status"), b.status}, {QStringLiteral("available"), b.levelAvailable}, {QStringLiteral("percent"), b.percent},
        {QStringLiteral("watts"), b.watts}, {QStringLiteral("powerAvailable"), b.powerAvailable},
        {QStringLiteral("healthAvailable"), b.healthAvailable}, {QStringLiteral("healthPercent"), b.healthPercent}});
    return result;
}
QVariantList PulseTelemetryController::capabilities() const
{
    QVariantList result;
    static const QHash<QString, QString> labels{
        {QStringLiteral("cpuLoad"), QStringLiteral("CPU load")}, {QStringLiteral("cpuTemperature"), QStringLiteral("CPU temperature")},
        {QStringLiteral("thermalHeadroom"), QStringLiteral("Thermal headroom")}, {QStringLiteral("gpuLoad"), QStringLiteral("GPU activity")},
        {QStringLiteral("gpuTemperature"), QStringLiteral("GPU temperature")}, {QStringLiteral("cpuPressure"), QStringLiteral("Raw CPU pressure")},
        {QStringLiteral("memoryPressure"), QStringLiteral("Raw memory pressure")}, {QStringLiteral("ioPressure"), QStringLiteral("Raw storage pressure")},
        {QStringLiteral("pressureIndex"), QStringLiteral("Responsiveness index")}, {QStringLiteral("memoryUsage"), QStringLiteral("Memory usage")},
        {QStringLiteral("zram"), QStringLiteral("ZRAM")}, {QStringLiteral("diskThroughput"), QStringLiteral("Storage throughput")},
        {QStringLiteral("netThroughput"), QStringLiteral("Network throughput")}, {QStringLiteral("packagePower"), QStringLiteral("CPU package power")},
        {QStringLiteral("fanSpeed"), QStringLiteral("Fan speed")}, {QStringLiteral("battery"), QStringLiteral("Battery status")},
        {QStringLiteral("batteryDischarge"), QStringLiteral("Battery discharge")}, {QStringLiteral("processes"), QStringLiteral("Process observations")},
        {QStringLiteral("powerProfile"), QStringLiteral("Power profiles")}};
    const auto metadata = metricMetadata();
    for (auto it = metadata.cbegin(); it != metadata.cend(); ++it) {
        const auto quality = it.value().toMap(); const QString state = quality.value(QStringLiteral("state")).toString();
        result.append(QVariantMap{{QStringLiteral("name"), labels.value(it.key(), it.key())}, {QStringLiteral("available"), state == QStringLiteral("available") || state == QStringLiteral("calculated")},
            {QStringLiteral("detail"), quality.value(QStringLiteral("detail"))}, {QStringLiteral("state"), state}});
    }
    return result;
}
QString PulseTelemetryController::collectorState() const
{
    if (!m_remoteClient) return m_recordingPaused ? QStringLiteral("Paused") : QStringLiteral("Live");
    if (m_receivedAt.isValid() && m_receivedAt.elapsed() > std::max(5000, m_timer.interval() * 3)) return QStringLiteral("Stale — last sample is old");
    return m_connectionState;
}
void PulseTelemetryController::setBackgroundMonitoring(bool value) { callCollector(QStringLiteral("SetBackground"), {value}); }
void PulseTelemetryController::setStartAtLogin(bool value) { callCollector(QStringLiteral("SetStartAtLogin"), {value}); }
void PulseTelemetryController::snoozeNotifications() { callCollector(QStringLiteral("Snooze"), {}, [this](const QByteArray &) { setActionResult(true, QStringLiteral("Notifications snoozed for 15 minutes.")); }); }
void PulseTelemetryController::quitMonitoring() { callCollector(QStringLiteral("StopMonitoring")); }

void PulseTelemetryController::rebuildHistoryView()
{
    m_historyPlotSamples.clear();
    m_historySamplesVariant.clear();
    for (int i = 0; i < m_recorder.count(); ++i) m_historySamplesVariant.append(sampleForView(m_recorder.sampleAt(i), i));
    m_incidentHistoryVariant = m_sessionMode ? m_session.incidents : (m_remoteClient ? m_remoteValues.value(QStringLiteral("recordedIncidents")).toList() : incidentRows(m_recorder));
}
void PulseTelemetryController::showSession(const SessionSnapshot &session, const QString &path, bool unsaved)
{
    if (session.samples.isEmpty()) { setActionResult(false, QStringLiteral("This incident has no retained samples.")); return; }
    if (!m_sessionMode) m_liveRecorder = m_recorder;
    ++m_investigationGeneration;
    m_session = session; m_sessionMode = true; m_sessionPath = path; m_unsavedInvestigation = unsaved;
    m_recorder.restoreSnapshot(session);
    m_selectedHistorySampleId = 0; m_selectedHistoryIndex = -1;
    m_investigationSummary = session.metadata.value(QStringLiteral("comparison")).toMap();
    rebuildHistoryView(); Q_EMIT telemetryUpdated(); Q_EMIT selectedHistoryChanged();
}
QByteArray PulseTelemetryController::collectorMarkProblem()
{
    const QString id = m_recorder.markProblemNow(QStringLiteral("Marked problem"));
    const auto bytes = collectorIncident(id);
    SessionSnapshot snapshot;
    if (SessionStore::decode(bytes, &snapshot)) {
        m_markedInvestigations.insert(id, snapshot); m_markedOrder.append(id);
        while (m_markedOrder.size() > 8) m_markedInvestigations.remove(m_markedOrder.takeFirst());
    }
    return bytes;
}
void PulseTelemetryController::markProblemNow()
{
    if (m_sessionMode) return;
    if (m_remoteClient) {
        callCollector(QStringLiteral("MarkProblem"), {}, [this](const QByteArray &data) {
            SessionSnapshot session; QString error;
            if (!SessionStore::decode(data, &session, &error)) { setActionResult(false, error); return; }
            showSession(session, {}, true);
            setActionResult(true, QStringLiteral("Problem marked. Save this frozen investigation to keep it."));
        });
        return;
    }
    m_recorder.markProblemNow(QStringLiteral("Marked problem"));
    if (!m_recorder.count()) { setActionResult(false, QStringLiteral("Wait for a recorded sample before marking a problem.")); return; }
    const auto id = m_recorder.samples().last().sampleId;
    showSession(m_recorder.freezeRange(id, id), {}, true);
}
void PulseTelemetryController::markProblemAndLaunch()
{
    callCollector(QStringLiteral("MarkProblem"), {}, [this](const QByteArray &data) {
        SessionSnapshot session; QString error;
        if (!SessionStore::decode(data, &session, &error)) { setActionResult(false, error); return; }
        launchIncident(session.metadata.value(QStringLiteral("markerId")).toString());
    });
}
void PulseTelemetryController::selectHistoryRange(qulonglong first, qulonglong last)
{
    if (first > last) std::swap(first, last);
    showSession(m_recorder.freezeRange(first, last), {}, true);
}
void PulseTelemetryController::selectIncident(const QString &id)
{
    if (m_sessionMode || !m_remoteClient) { showSession(m_recorder.freezeIncident(id), {}, true); return; }
    callCollector(QStringLiteral("Incident"), {id}, [this](const QByteArray &data) {
        SessionSnapshot session; QString error;
        if (!SessionStore::decode(data, &session, &error)) { setActionResult(false, error); return; }
        showSession(session, {}, true);
    });
}
void PulseTelemetryController::launchIncident(const QString &id)
{
    callCollector(QStringLiteral("OpenIncident"), {id}, [this](const QByteArray &) {
        setActionResult(true, QStringLiteral("Opened the recorded incident in the application matching the collector."));
    });
}
void PulseTelemetryController::returnToLive()
{
    if (!m_sessionMode) { setSelectedHistorySampleId(0); return; }
    ++m_investigationGeneration;
    m_sessionMode = false; m_openedSession = false; m_sourceSession = {}; m_session = {}; m_sessionPath.clear(); m_unsavedInvestigation = false;
    m_recorder = m_liveRecorder; m_selectedHistorySampleId = 0; m_selectedHistoryIndex = -1;
    m_investigationSummary.clear(); rebuildHistoryView(); Q_EMIT telemetryUpdated(); Q_EMIT selectedHistoryChanged();
}
void PulseTelemetryController::saveSession(const QString &path)
{
    if (m_actionPending) return;
    const QString file = localPath(path);
    const auto snapshot = m_sessionMode ? m_session : (m_recorder.count() ? m_recorder.freezeRange(m_recorder.samples().first().sampleId, m_recorder.samples().last().sampleId) : SessionSnapshot{});
    const auto generation = m_investigationGeneration;
    const bool include = m_exportIncludeProcessDetails;
    m_actionPending = true; Q_EMIT telemetryUpdated();
    QPointer<PulseTelemetryController> guard(this);
    m_filePool.start(QRunnable::create([guard, file, snapshot, include, generation] {
        QString error; const bool ok = SessionStore::write(file, snapshot, include, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard, [guard, file, ok, error, generation] {
            if (!guard) return;
            guard->m_actionPending = false;
            if (ok && guard->m_investigationGeneration == generation) { guard->m_unsavedInvestigation = false; guard->m_sessionPath = file; guard->m_reportsDirectory = QFileInfo(file).absolutePath(); }
            guard->setActionResult(ok, ok ? QStringLiteral("Session saved.") : error, ok ? file : QString());
            Q_EMIT guard->telemetryUpdated();
        }, Qt::QueuedConnection);
    }));
}
void PulseTelemetryController::openSession(const QString &path)
{
    if (m_actionPending) return;
    const QString file = localPath(path);
    const auto generation = m_investigationGeneration;
    m_actionPending = true; Q_EMIT telemetryUpdated();
    QPointer<PulseTelemetryController> guard(this);
    m_filePool.start(QRunnable::create([guard, file, generation] {
        SessionSnapshot snapshot; QString error; const bool ok = SessionStore::read(file, &snapshot, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard, [guard, file, ok, snapshot, error, generation] {
            if (!guard) return;
            guard->m_actionPending = false;
            if (ok && guard->m_investigationGeneration == generation) { guard->m_openedSession = true; guard->m_sourceSession = snapshot; guard->showSession(snapshot, file, false); }
            guard->setActionResult(ok, ok ? QStringLiteral("Opened a saved session. Live recording continues separately.") : error, ok ? file : QString());
            Q_EMIT guard->telemetryUpdated();
        }, Qt::QueuedConnection);
    }));
}
QString PulseTelemetryController::exportEntireRecording()
{
    if (!m_sessionMode) return exportIncidentReportToFile();
    const auto frozen = m_recorder;
    if (m_openedSession) {
        m_recorder.restoreSnapshot(m_sourceSession);
    } else m_recorder = m_liveRecorder;
    QString path;
    if (m_remoteClient) exportRecorderAsync(m_recorder);
    else path = exportIncidentReportToFile();
    m_recorder = frozen;
    return path;
}
QString PulseTelemetryController::exportSelectedIncident()
{
    if (!m_sessionMode) { setActionResult(false, QStringLiteral("Select an incident or time range first.")); return {}; }
    return exportIncidentReportToFile();
}
}
