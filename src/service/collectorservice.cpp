#include "collectorservice.h"
#include "collectorprotocol.h"
#include <KNotification>
#include <QCoreApplication>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

namespace LoofiPulse {
namespace {
QByteArray success() { return QByteArrayLiteral("{}"); }
}
CollectorService::CollectorService(QObject *parent) : QObject(parent)
{
    m_loginMonitoring = QCoreApplication::arguments().contains(QStringLiteral("--background"))
        && m_settings.value(QStringLiteral("startAtLogin"), false).toBool();
    m_anchor = new QObject;
    m_anchor->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_anchor, &QObject::deleteLater);
    connect(&m_thread, &QThread::started, m_anchor, [this] {
        m_engine = new PulseTelemetryController(QDBusConnection::systemBus(), m_anchor);
        connect(m_engine, &PulseTelemetryController::telemetryUpdated, m_anchor, [this] {
            const auto packet = m_engine->collectorPacket();
            QMetaObject::invokeMethod(this, [this, packet] { publish(packet); }, Qt::QueuedConnection);
        });
        const auto packet = m_engine->collectorPacket();
        QMetaObject::invokeMethod(this, [this, packet] { publish(packet); }, Qt::QueuedConnection);
    });
    m_idle.setSingleShot(true); m_idle.setInterval(30000);
    connect(&m_idle, &QTimer::timeout, this, [] { QCoreApplication::quit(); });
    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"), this,
        SLOT(ownerChanged(QString,QString,QString)));
    QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("PrepareForSleep"), this,
        SLOT(onPrepareForSleep(bool)));
    m_busHealth.setInterval(500);
    connect(&m_busHealth, &QTimer::timeout, this, [] {
        if (!QDBusConnection::sessionBus().isConnected()) QCoreApplication::quit();
    });
    m_busHealth.start();
    m_thread.start();
    updateRequests();
}
bool CollectorService::backgroundActive() const { return m_loginMonitoring || m_settings.value(QStringLiteral("backgroundMonitoring"), false).toBool(); }
CollectorService::~CollectorService() { m_thread.quit(); m_thread.wait(); }

QByteArray CollectorService::dispatch(std::function<QByteArray(PulseTelemetryController *)> operation)
{
    const auto request = message();
    setDelayedReply(true);
    QMetaObject::invokeMethod(m_anchor, [this, request, operation] {
        const QByteArray result = operation(m_engine);
        QMetaObject::invokeMethod(this, [request, result] {
            QDBusConnection::sessionBus().send(request.createReply(QVariantList{result}));
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    return {};
}
QByteArray CollectorService::Subscribe(const QString &id, int interval, int retention, bool notifications)
{
    if (id.isEmpty() || id.size() > 128 || m_clients.size() > 128) {
        sendErrorReply(QStringLiteral("com.github.loofi.hardwarepulse.InvalidClient"), QStringLiteral("Invalid client subscription.")); return {};
    }
    const QString key = message().service() + QLatin1Char('/') + id;
    m_clients.insert(key, {message().service(), std::clamp(interval, 250, 10000), std::clamp(retention, 300, 1800), notifications});
    updateRequests();
    return dispatch([](auto *engine) { return engine->collectorPacket(true); });
}
QByteArray CollectorService::Unsubscribe(const QString &id)
{
    m_clients.remove(message().service() + QLatin1Char('/') + id); updateRequests(); return success();
}
void CollectorService::ownerChanged(const QString &name, const QString &, const QString &owner)
{
    if (!owner.isEmpty()) return;
    bool removed = false;
    for (auto it = m_clients.begin(); it != m_clients.end();) {
        if (it->owner == name) { it = m_clients.erase(it); removed = true; } else ++it;
    }
    if (removed) updateRequests();
}
void CollectorService::updateRequests()
{
    const bool background = backgroundActive();
    if (m_clients.isEmpty() && !background) { if (!m_idle.isActive()) m_idle.start(); } else m_idle.stop();
    if (!m_clients.isEmpty()) { bool enabled = false; for (const auto &client : std::as_const(m_clients)) enabled = enabled || client.notifications; m_settings.setValue(QStringLiteral("backgroundNotificationsEnabled"), enabled); }
    int interval = background ? 1000 : 10000;
    int retention = 300;
    for (const auto &client : std::as_const(m_clients)) { interval = std::min(interval, client.interval); retention = std::max(retention, client.retention); }
    QMetaObject::invokeMethod(m_anchor, [this, interval, retention] {
        m_engine->setPollIntervalMs(interval); m_engine->setHistoryRetentionSeconds(retention);
    }, Qt::QueuedConnection);
}
QByteArray CollectorService::History(qulonglong afterId, int limit) { return dispatch([=](auto *e) { return e->collectorHistory(afterId, limit); }); }
QByteArray CollectorService::Incident(const QString &id) { return dispatch([id](auto *e) { return e->collectorIncident(id); }); }
QByteArray CollectorService::MarkProblem() { return dispatch([](auto *e) { return e->collectorMarkProblem(); }); }

void CollectorService::onPrepareForSleep(bool goingToSleep)
{
    if (goingToSleep) {
        QMetaObject::invokeMethod(m_anchor, [this] {
            if (m_engine) m_engine->setRecordingPaused(true);
        }, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(m_anchor, [this] {
            if (m_engine) {
                m_engine->handleSystemResume();
                m_engine->setRecordingPaused(false);
            }
        }, Qt::QueuedConnection);
    }
}

QByteArray CollectorService::OpenApplication(const QString &page, const QString &section)
{
    const QString target = PulseTelemetryController::normalizedPageName(page);
    QStringList arguments{QStringLiteral("--page"), target};
    const QString validSection = PulseTelemetryController::validatedSectionName(target, section);
    if (!validSection.isEmpty()) arguments << QStringLiteral("--section") << validSection;

    auto bus = QDBusConnection::sessionBus();
    auto call = QDBusMessage::createMethodCall(QStringLiteral("com.github.loofi.hardwarepulse.App"),
        QStringLiteral("/com/github/loofi/hardwarepulse/App"),
        QStringLiteral("com.github.loofi.hardwarepulse.App1"), QStringLiteral("Activate"));
    call << arguments << QDir::currentPath() << QString();
    auto reply = bus.call(call, QDBus::Block, 1000);
    if (reply.type() == QDBusMessage::ReplyMessage) return success();

    const QString executable = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("loofi-pulse"));
    if (!QFileInfo(executable).isExecutable() || !QProcess::startDetached(executable, arguments)) {
        sendErrorReply(QStringLiteral("com.github.loofi.hardwarepulse.LaunchFailed"), QStringLiteral("The application matching this collector could not be opened.")); return {};
    }
    return success();
}
QByteArray CollectorService::OpenIncident(const QString &id)
{
    if (id.isEmpty() || id.size() > 200) {
        sendErrorReply(QStringLiteral("com.github.loofi.hardwarepulse.LaunchFailed"), QStringLiteral("The recorded incident could not be opened in the matching application.")); return {};
    }
    const QStringList arguments{QStringLiteral("--incident"), id};
    auto bus = QDBusConnection::sessionBus();
    auto call = QDBusMessage::createMethodCall(QStringLiteral("com.github.loofi.hardwarepulse.App"),
        QStringLiteral("/com/github/loofi/hardwarepulse/App"),
        QStringLiteral("com.github.loofi.hardwarepulse.App1"), QStringLiteral("Activate"));
    call << arguments << QDir::currentPath() << QString();
    auto reply = bus.call(call, QDBus::Block, 1000);
    if (reply.type() == QDBusMessage::ReplyMessage) return success();

    const QString executable = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("loofi-pulse"));
    if (!QFileInfo(executable).isExecutable()
        || !QProcess::startDetached(executable, arguments)) {
        sendErrorReply(QStringLiteral("com.github.loofi.hardwarepulse.LaunchFailed"), QStringLiteral("The recorded incident could not be opened in the matching application.")); return {};
    }
    return success();
}

QByteArray CollectorService::ClearHistory() { return dispatch([](auto *e) { e->clearHistory(); return success(); }); }
QByteArray CollectorService::SetBackground(bool enabled)
{
    m_loginMonitoring = false;
    m_settings.setValue(QStringLiteral("backgroundMonitoring"), enabled); m_settings.sync(); updateRequests();
    if (!m_latest.isEmpty()) publish(m_latest);
    return success();
}
QByteArray CollectorService::SetStartAtLogin(bool enabled)
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart");
    const QString path = directory + QStringLiteral("/loofi-pulse-monitor.desktop");
    if (enabled) {
        QDir().mkpath(directory);
        QSaveFile file(path);
        QString executable = QCoreApplication::applicationFilePath();
        executable.replace(QLatin1Char('\\'), QStringLiteral("\\\\")); executable.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        const QByteArray contents = QStringLiteral("[Desktop Entry]\nType=Application\nName=Loofi Pulse monitoring\nExec=\"%1\" --background\nTerminal=false\nOnlyShowIn=KDE;\n").arg(executable).toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() || !file.commit()) {
            sendErrorReply(QStringLiteral("com.github.loofi.hardwarepulse.SaveFailed"), QStringLiteral("Could not save login startup preference.")); return {};
        }
    } else if (QFile::exists(path) && !QFile::remove(path)) {
        sendErrorReply(QStringLiteral("com.github.loofi.hardwarepulse.SaveFailed"), QStringLiteral("Could not remove Pulse login startup entry.")); return {};
    }
    m_settings.setValue(QStringLiteral("startAtLogin"), enabled); m_settings.sync();
    if (!m_latest.isEmpty()) publish(m_latest);
    return success();
}
QByteArray CollectorService::Snooze() { m_snoozeUntil = QDateTime::currentMSecsSinceEpoch() + 900000; return success(); }
QByteArray CollectorService::StopMonitoring()
{
    m_loginMonitoring = false;
    m_settings.setValue(QStringLiteral("backgroundMonitoring"), false); m_settings.sync();
    // Existing widget clients continue to receive telemetry; closing the last client starts idle shutdown.
    updateRequests(); if (!m_latest.isEmpty()) publish(m_latest); return success();
}
void CollectorService::publish(const QByteArray &bytes)
{
    auto packet = QJsonDocument::fromJson(bytes).object().toVariantMap();
    auto live = packet.value(QStringLiteral("live")).toMap();
    live[QStringLiteral("backgroundMonitoring")] = backgroundActive();
    live[QStringLiteral("startAtLogin")] = m_settings.value(QStringLiteral("startAtLogin"), false);
    packet[QStringLiteral("live")] = live;
    m_latest = QJsonDocument(QJsonObject::fromVariantMap(packet)).toJson(QJsonDocument::Compact);
    notify(packet); Q_EMIT FrameReady(m_latest);
}
void CollectorService::notify(const QVariantMap &packet)
{
    bool enabled = m_clients.isEmpty() && backgroundActive()
        && m_settings.value(QStringLiteral("backgroundNotificationsEnabled"), true).toBool();
    for (const auto &client : std::as_const(m_clients)) enabled = enabled || client.notifications;
    if (!enabled || QDateTime::currentMSecsSinceEpoch() < m_snoozeUntil) return;
    const auto live = packet.value(QStringLiteral("live")).toMap();
    const int severity = live.value(QStringLiteral("verdictSeverity")).toInt();
    if (severity != 2 && severity != 3) return;
    const auto incidents = packet.value(QStringLiteral("incidents")).toList();
    QString id;
    for (const auto &entry : incidents) { const auto episode = entry.toMap(); if (episode.value(QStringLiteral("active")).toBool()) { id = episode.value(QStringLiteral("incidentId")).toString(); break; } }
    if (id.isEmpty() || (m_notified.contains(id) && (severity < 3 || m_notified.value(id) >= 3))) return;
    m_notified[id] = severity;
    // Bound deduplication state; completed episodes outside retention cannot reappear.
    if (m_notified.size() > 256) { QSet<QString> retained; for (const auto &entry : incidents) retained.insert(entry.toMap().value(QStringLiteral("incidentId")).toString()); for (auto it = m_notified.begin(); it != m_notified.end();) { if (!retained.contains(it.key())) it = m_notified.erase(it); else ++it; } }
    auto *notification = new KNotification(severity == 3 ? QStringLiteral("thermalThrottle") : QStringLiteral("psiBottleneck"), KNotification::CloseOnTimeout, this);
    notification->setComponentName(QStringLiteral("plasma_applet_com.github.loofi.hardwarepulse"));
    notification->setTitle(live.value(QStringLiteral("verdictHeadline")).toString());
    notification->setText(live.value(QStringLiteral("verdictCausalChain")).toString());
    auto *open = notification->addAction(QStringLiteral("Open incident"));
    auto *snooze = notification->addAction(QStringLiteral("Snooze 15 minutes"));
    connect(open, &KNotificationAction::activated, this, [id] {
        const QStringList arguments{QStringLiteral("--incident"), id};
        auto bus = QDBusConnection::sessionBus();
        auto call = QDBusMessage::createMethodCall(QStringLiteral("com.github.loofi.hardwarepulse.App"),
            QStringLiteral("/com/github/loofi/hardwarepulse/App"),
            QStringLiteral("com.github.loofi.hardwarepulse.App1"), QStringLiteral("Activate"));
        call << arguments << QDir::currentPath() << QString();
        auto reply = bus.call(call, QDBus::Block, 1000);
        if (reply.type() == QDBusMessage::ReplyMessage) return;

        const QString executable = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("loofi-pulse"));
        QProcess::startDetached(executable, arguments);
    });
    connect(snooze, &KNotificationAction::activated, this, [this] { m_snoozeUntil = QDateTime::currentMSecsSinceEpoch() + 900000; });
    notification->sendEvent();
}
}
