#include "pulsetelemetrycontroller.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace LoofiPulse;

class FakePowerProfiles : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.hadess.PowerProfiles")
    Q_PROPERTY(QString ActiveProfile READ activeProfile)
    Q_PROPERTY(QStringList Profiles READ profiles)

public:
    explicit FakePowerProfiles(QObject *parent = nullptr) : QObject(parent) {}
    QString activeProfile() const { return QStringLiteral("balanced"); }
    QStringList profiles() const { return {QStringLiteral("balanced"), QStringLiteral("power-saver")}; }
};

class TestControllerActions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testMissingSystemMonitorReportsFailure()
    {
        const QByteArray previousPath = qgetenv("PATH");
        QTemporaryDir emptyPath;
        QVERIFY(emptyPath.isValid());
        qputenv("PATH", QFile::encodeName(emptyPath.path()));
        PulseTelemetryController controller;
        QVERIFY(!controller.openSystemMonitor());
        QVERIFY(!controller.lastActionSucceeded());
        QVERIFY(controller.lastActionStatus().contains(QStringLiteral("not installed")));
        qputenv("PATH", previousPath);
    }

    void testUnsupportedProfileNeverStartsChange()
    {
        PulseTelemetryController controller;
        QVERIFY(!controller.setPowerProfile(QStringLiteral("not-a-real-power-profile")));
        QVERIFY(!controller.powerProfileChangePending());
        QVERIFY(!controller.lastActionSucceeded());
        QVERIFY(controller.lastActionStatus().contains(QStringLiteral("not available")));
    }

    void testAsyncProfileSetFailure()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FakePowerProfiles fake;
        QVERIFY(bus.registerService(QStringLiteral("net.hadess.PowerProfiles")));
        QVERIFY(bus.registerObject(QStringLiteral("/net/hadess/PowerProfiles"), &fake, QDBusConnection::ExportAllProperties));

        QDBusMessage get = QDBusMessage::createMethodCall(QStringLiteral("net.hadess.PowerProfiles"),
            QStringLiteral("/net/hadess/PowerProfiles"), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
        get << QStringLiteral("net.hadess.PowerProfiles") << QStringLiteral("ActiveProfile");
        const QDBusMessage response = bus.call(get);
        QVERIFY2(response.type() == QDBusMessage::ReplyMessage, qPrintable(response.errorMessage()));

        PulseTelemetryController controller(bus);
        QTRY_VERIFY_WITH_TIMEOUT(controller.powerProfileServiceAvailable(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(controller.availablePowerProfiles().contains(QStringLiteral("power-saver")), 2000);
        QVERIFY(controller.setPowerProfile(QStringLiteral("power-saver")));
        QVERIFY(controller.powerProfileChangePending());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.powerProfileChangePending(), 2000);
        QCOMPARE(fake.activeProfile(), QStringLiteral("balanced"));
        QVERIFY(!controller.lastActionSucceeded());
        QVERIFY(controller.lastActionStatus().contains(QStringLiteral("failed")));

        bus.unregisterObject(QStringLiteral("/net/hadess/PowerProfiles"));
        bus.unregisterService(QStringLiteral("net.hadess.PowerProfiles"));
    }

    void testUnwritableReportLocationReportsFailure()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString blockedPath = tempDir.filePath(QStringLiteral("blocked"));
        QFile file(blockedPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        const QByteArray previousDataHome = qgetenv("XDG_DATA_HOME");
        qputenv("XDG_DATA_HOME", QFile::encodeName(blockedPath));
        PulseTelemetryController controller;
        QCOMPARE(controller.exportIncidentReportToFile(), QString());
        QVERIFY(!controller.lastActionSucceeded());
        QVERIFY(controller.lastActionStatus().contains(QStringLiteral("could not be created")));
        qputenv("XDG_DATA_HOME", previousDataHome);
    }
};

QTEST_MAIN(TestControllerActions)
#include "test_controller_actions.moc"
