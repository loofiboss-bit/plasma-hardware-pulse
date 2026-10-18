#include "collectorprotocol.h"
#include <QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QEventLoop>
#include <QTimer>
#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

using namespace LoofiPulse;
class NotificationServer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    struct Entry { uint id; QString summary; QString body; QStringList actions; };
    QList<Entry> entries;
public Q_SLOTS:
    QStringList GetCapabilities() const { return {QStringLiteral("actions"), QStringLiteral("body"), QStringLiteral("persistence")}; }
    QString GetServerInformation(QString &vendor, QString &version, QString &specVersion) const
    {
        vendor = QStringLiteral("Loofi Pulse test fixture"); version = QStringLiteral("1.0"); specVersion = QStringLiteral("1.2");
        return QStringLiteral("Pulse isolated test notification server");
    }
    uint Notify(const QString &, uint replacesId, const QString &, const QString &summary, const QString &body,
                const QStringList &actions, const QVariantMap &, int)
    {
        const uint id = replacesId ? replacesId : uint(entries.size() + 1);
        entries.append({id, summary, body, actions});
        return id;
    }
    void CloseNotification(uint id) { Q_EMIT NotificationClosed(id, 3); }
Q_SIGNALS:
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &action);
};
class NotificationsV09Test : public QObject
{
    Q_OBJECT
    QTemporaryDir fixture;
    QProcess helper;
    NotificationServer server;
    QVariantMap latest;
    bool put(const QString &relative, const QByteArray &contents)
    {
        const QString path = fixture.filePath(relative);
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
    }
    bool memoryPressure(bool high)
    {
        return put(QStringLiteral("proc/pressure/memory"), high
            ? QByteArrayLiteral("some avg10=20.00 avg60=15.00 avg300=10.00 total=200000\nfull avg10=5.00 avg60=4.00 avg300=3.00 total=50000\n")
            : QByteArrayLiteral("some avg10=0.00 avg60=0.00 avg300=0.00 total=200000\nfull avg10=0.00 avg60=0.00 avg300=0.00 total=50000\n"));
    }
    QByteArray request(const QString &method, const QVariantList &arguments = {})
    {
        auto message = QDBusMessage::createMethodCall(QString::fromLatin1(CollectorServiceName), QString::fromLatin1(CollectorObjectPath), QString::fromLatin1(CollectorInterface), method);
        message.setArguments(arguments);
        // Keep servicing the fake notification endpoint while the real helper handles IPC.
        QDBusPendingCallWatcher pending(QDBusConnection::sessionBus().asyncCall(message, 5000));
        QEventLoop loop;
        connect(&pending, &QDBusPendingCallWatcher::finished, &loop, &QEventLoop::quit);
        if (!pending.isFinished()) loop.exec();
        const auto reply = pending.reply();
        if (reply.type() == QDBusMessage::ErrorMessage) { qWarning() << method << reply.errorName() << reply.errorMessage(); return {}; }
        return reply.arguments().value(0).toByteArray();
    }
    bool subscribe(const QString &id, bool notifications)
    {
        return !request(QStringLiteral("Subscribe"), {id, 250, 300, notifications}).isEmpty();
    }
    quint64 sampleId() const { return latest.value(QStringLiteral("sample")).toMap().value(QStringLiteral("sampleId")).toULongLong(); }
    QString activeIncident() const
    {
        for (const auto &entry : latest.value(QStringLiteral("incidents")).toList()) {
            const auto episode = entry.toMap();
            if (episode.value(QStringLiteral("active")).toBool()) return episode.value(QStringLiteral("incidentId")).toString();
        }
        return {};
    }
    bool warning() const
    {
        return latest.value(QStringLiteral("live")).toMap().value(QStringLiteral("verdictRuleId")).toString() == QStringLiteral("memory.pressure")
            && latest.value(QStringLiteral("live")).toMap().value(QStringLiteral("verdictSeverity")).toInt() == 2
            && !activeIncident().isEmpty();
    }
private Q_SLOTS:
    void frame(const QByteArray &bytes) { latest = QJsonDocument::fromJson(bytes).object().toVariantMap(); }
    void initTestCase()
    {
        QVERIFY(fixture.isValid());
        auto bus = QDBusConnection::sessionBus();
        QVERIFY2(bus.isConnected(), "Run this test under dbus-run-session.");
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.Notifications")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/freedesktop/Notifications"), &server, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
        QVERIFY(bus.connect(QString::fromLatin1(CollectorServiceName), QString::fromLatin1(CollectorObjectPath), QString::fromLatin1(CollectorInterface), QStringLiteral("FrameReady"), this, SLOT(frame(QByteArray))));
        QVERIFY(put(QStringLiteral("proc/stat"), "cpu 1000 0 0 1000 0 0 0 0\ncpu0 1000 0 0 1000 0 0 0 0\n"));
        QVERIFY(put(QStringLiteral("proc/meminfo"), "MemTotal: 16000000 kB\nMemAvailable: 8000000 kB\nSwapTotal: 0 kB\nSwapFree: 0 kB\n"));
        QVERIFY(put(QStringLiteral("proc/pressure/cpu"), "some avg10=0.00 avg60=0.00 avg300=0.00 total=0\n"));
        QVERIFY(put(QStringLiteral("proc/pressure/io"), "some avg10=0.00 avg60=0.00 avg300=0.00 total=0\nfull avg10=0.00 avg60=0.00 avg300=0.00 total=0\n"));
        QVERIFY(memoryPressure(false));
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("LOOFI_PULSE_SYS_ROOT"), fixture.filePath(QStringLiteral("sys")));
        environment.insert(QStringLiteral("LOOFI_PULSE_PROC_ROOT"), fixture.filePath(QStringLiteral("proc")));
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), fixture.filePath(QStringLiteral("config")));
        environment.insert(QStringLiteral("XDG_DATA_HOME"), fixture.filePath(QStringLiteral("data")));
        environment.insert(QStringLiteral("XDG_CACHE_HOME"), fixture.filePath(QStringLiteral("cache")));
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        helper.setProcessEnvironment(environment);
        helper.start(QStringLiteral(PULSE_SERVICE_BIN));
        QVERIFY(helper.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(bus.interface()->isServiceRegistered(QString::fromLatin1(CollectorServiceName)).value(), 5000);
    }
    void actualServiceDeduplicationPreferencesAndSnooze()
    {
        QVERIFY(subscribe(QStringLiteral("app"), true));
        QVERIFY(subscribe(QStringLiteral("widget"), true));
        QTRY_VERIFY_WITH_TIMEOUT(sampleId() > 0, 5000);
        QCOMPARE(server.entries.size(), 0);
        QVERIFY(memoryPressure(true));
        QTRY_VERIFY_WITH_TIMEOUT(warning(), 12000);
        const QString firstIncident = activeIncident();
        QTRY_COMPARE_WITH_TIMEOUT(server.entries.size(), 1, 5000);
        const auto first = server.entries.first();
        QVERIFY(first.summary.contains(QStringLiteral("Memory pressure")));
        QVERIFY(first.actions.contains(QStringLiteral("Open incident")));
        QVERIFY(first.actions.contains(QStringLiteral("Snooze 15 minutes")));
        const quint64 firstSample = sampleId();
        QTRY_VERIFY_WITH_TIMEOUT(sampleId() >= firstSample + 5, 5000);
        QCOMPARE(server.entries.size(), 1);
        QCOMPARE(activeIncident(), firstIncident);

        QVERIFY(subscribe(QStringLiteral("app"), false));
        QVERIFY(subscribe(QStringLiteral("widget"), false));
        QVERIFY(!request(QStringLiteral("SetBackground"), {true}).isEmpty());
        QVERIFY(memoryPressure(false));
        QTRY_VERIFY_WITH_TIMEOUT(!warning() && activeIncident().isEmpty(), 8000);
        QVERIFY(memoryPressure(true));
        QTRY_VERIFY_WITH_TIMEOUT(warning() && activeIncident() != firstIncident, 12000);
        const QString disabledIncident = activeIncident();
        QVERIFY(!request(QStringLiteral("SetBackground"), {false}).isEmpty());
        QVERIFY(!request(QStringLiteral("SetBackground"), {true}).isEmpty());
        const quint64 disabledSample = sampleId();
        QTRY_VERIFY_WITH_TIMEOUT(sampleId() >= disabledSample + 3, 5000);
        QCOMPARE(server.entries.size(), 1);
        // No subscribers: the saved opt-out must also govern background monitoring.
        QVERIFY(!request(QStringLiteral("Unsubscribe"), {QStringLiteral("app")}).isEmpty());
        QVERIFY(!request(QStringLiteral("Unsubscribe"), {QStringLiteral("widget")}).isEmpty());
        const quint64 backgroundSample = sampleId();
        QTRY_VERIFY_WITH_TIMEOUT(sampleId() >= backgroundSample + 2, 5000);
        QCOMPARE(server.entries.size(), 1);
        QVERIFY(subscribe(QStringLiteral("app"), true));
        QVERIFY(subscribe(QStringLiteral("widget"), true));
        QTRY_COMPARE_WITH_TIMEOUT(server.entries.size(), 2, 5000);
        QVERIFY(!request(QStringLiteral("Snooze")).isEmpty());
        QVERIFY(memoryPressure(false));
        QTRY_VERIFY_WITH_TIMEOUT(!warning() && activeIncident().isEmpty(), 8000);
        QVERIFY(memoryPressure(true));
        QTRY_VERIFY_WITH_TIMEOUT(warning() && activeIncident() != disabledIncident, 12000);
        const quint64 snoozedSample = sampleId();
        QTRY_VERIFY_WITH_TIMEOUT(sampleId() >= snoozedSample + 5, 5000);
        QCOMPARE(server.entries.size(), 2);
        qInfo() << "Actual service: two subscriptions produced one notification per episode; all-client opt-out survived background toggles and no subscribers; Snooze suppressed a new confirmed memory-pressure episode; Open incident and Snooze 15 minutes actions were present.";
    }
    void cleanupTestCase()
    {
        if (helper.state() != QProcess::NotRunning) { helper.terminate(); if (!helper.waitForFinished(5000)) { helper.kill(); helper.waitForFinished(5000); } }
        QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/org/freedesktop/Notifications"));
        QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.freedesktop.Notifications"));
    }
};
QTEST_GUILESS_MAIN(NotificationsV09Test)
#include "test_notifications_v09.moc"
