#include "pulsetelemetrycontroller.h"
#include "collectorprotocol.h"
#include "historystore.h"
#include <QDBusContext>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <unistd.h>

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

class FakeTrendsCollector : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.github.loofi.hardwarepulse.Collector1")
public:
    bool failNext = false;
    bool dropNext = false;
    QStringList requestedMetrics;
    qint64 trendsFrom = 0, trendsTo = 0, incidentsFrom = 0, incidentsTo = 0;
public Q_SLOTS:
    QByteArray Trends(qint64 from, qint64 to, int, const QStringList &metrics)
    {
        requestedMetrics = metrics;
        trendsFrom = from; trendsTo = to;
        if (failNext) {
            failNext = false;
            sendErrorReply(QStringLiteral("com.github.loofi.hardwarepulse.TestFailure"), QStringLiteral("Storage unavailable"));
            return {};
        }
        const auto request = message();
        const auto replyBus = connection();
        setDelayedReply(true);
        if (dropNext) { dropNext = false; return {}; }
        const auto response = QJsonDocument(QJsonObject{{QStringLiteral("fromMs"), from},
            {QStringLiteral("toMs"), to}, {QStringLiteral("series"), QJsonObject()}}).toJson();
        QTimer::singleShot(from == 100 ? 150 : 10, this, [request, response, replyBus] {
            replyBus.send(request.createReply(QVariantList{response}));
        });
        return {};
    }
    QByteArray ClearPersistedHistory()
    {
        const auto request = message();
        const auto replyBus = connection();
        setDelayedReply(true);
        QTimer::singleShot(120, this, [request, replyBus] {
            replyBus.send(request.createReply(QVariantList{QByteArrayLiteral("{}")}));
        });
        return {};
    }
    QByteArray PersistedIncidents(qint64 from, qint64 to, int)
    {
        incidentsFrom = from; incidentsTo = to;
        return QJsonDocument(QJsonArray{QJsonObject{{QStringLiteral("incidentId"), QStringLiteral("stored-incident")}}}).toJson();
    }
};

class TestControllerActions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testNavigationValidationAndPrivacyDefaults()
    {
        QCOMPARE(PulseTelemetryController::normalizedPageName(QStringLiteral(" power ")), QStringLiteral("power"));
        QCOMPARE(PulseTelemetryController::normalizedPageName(QStringLiteral("storage")), QStringLiteral("storage"));
        QCOMPARE(PulseTelemetryController::normalizedPageName(QStringLiteral("devices")), QStringLiteral("storage"));
        QCOMPARE(PulseTelemetryController::normalizedPageName(QStringLiteral("unknown")), QStringLiteral("overview"));
        QCOMPARE(PulseTelemetryController::validatedSectionName(QStringLiteral("power"), QStringLiteral("gpu")), QStringLiteral("gpu"));
        QCOMPARE(PulseTelemetryController::validatedSectionName(QStringLiteral("pressure"), QStringLiteral("network")), QStringLiteral("network"));
        QVERIFY(PulseTelemetryController::validatedSectionName(QStringLiteral("power"), QStringLiteral("processes")).isEmpty());
        PulseTelemetryController controller(QDBusConnection::systemBus());
        QVERIFY(!controller.exportIncludeProcessDetails());
        QSignalSpy changes(&controller, &PulseTelemetryController::exportIncludeProcessDetailsChanged);
        controller.setExportIncludeProcessDetails(true);
        QVERIFY(controller.exportIncludeProcessDetails());
        QCOMPARE(changes.count(), 1);
        controller.setExportIncludeProcessDetails(true);
        QCOMPARE(changes.count(), 1);
    }

    void testNetworkSelectionAndHistoryMetadata()
    {
        PulseTelemetryController controller(QDBusConnection::systemBus());
        QVERIFY(controller.selectedNetworkInterfaces().isEmpty());
        QSignalSpy changes(&controller, &PulseTelemetryController::selectedNetworkInterfacesChanged);
        controller.setSelectedNetworkInterfaces({QStringLiteral(" tun0 "), QStringLiteral("eth0"), QStringLiteral("tun0"), QString()});
        QCOMPARE(controller.selectedNetworkInterfaces(), QStringList({QStringLiteral("eth0"), QStringLiteral("tun0")}));
        QCOMPARE(changes.count(), 1);
        controller.setSelectedNetworkInterfaces({QStringLiteral("tun0"), QStringLiteral("eth0")});
        QCOMPARE(changes.count(), 1);
        const auto sample = controller.historySamples().last().toMap();
        QVERIFY(sample.contains(QStringLiteral("packageWatts")));
        QVERIFY(sample.contains(QStringLiteral("batteryWatts")));
        const auto quality = sample.value(QStringLiteral("metricQuality")).toMap();
        const auto metadata = quality.value(QStringLiteral("metricMetadata")).toMap();
        QVERIFY(metadata.contains(QStringLiteral("batteryDischarge")));
        QVERIFY(metadata.contains(QStringLiteral("processes")));
        QCOMPARE(metadata.value(QStringLiteral("packagePower")).toMap().value(QStringLiteral("source")),
            controller.metricMetadata().value(QStringLiteral("packagePower")).toMap().value(QStringLiteral("source")));
        controller.setRecordingPaused(true);
        controller.refreshNow();
        controller.setRecordingPaused(false);
        controller.refreshNow();
        QVERIFY(controller.historySamples().last().toMap().value(QStringLiteral("gapBefore")).toBool());
        controller.clearHistory();
        QCOMPARE(controller.actualHistorySeconds(), 0.0);
        controller.setSelectedNetworkInterfaces({});
        QVERIFY(controller.selectedNetworkInterfaces().isEmpty());
    }

    void testMissingSystemMonitorReportsFailure()
    {
        const QByteArray previousPath = qgetenv("PATH");
        QTemporaryDir emptyPath;
        QVERIFY(emptyPath.isValid());
        qputenv("PATH", QFile::encodeName(emptyPath.path()));
        PulseTelemetryController controller(QDBusConnection::systemBus());
        QVERIFY(!controller.openSystemMonitor());
        QVERIFY(!controller.lastActionSucceeded());
        QVERIFY(controller.lastActionStatus().contains(QStringLiteral("not installed")));
        qputenv("PATH", previousPath);
    }

    void testUnsupportedProfileNeverStartsChange()
    {
        PulseTelemetryController controller(QDBusConnection::systemBus());
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
        PulseTelemetryController controller(QDBusConnection::systemBus());
        QCOMPARE(controller.exportIncidentReportToFile(), QString());
        QVERIFY(!controller.lastActionSucceeded());
        QVERIFY(controller.lastActionStatus().contains(QStringLiteral("could not be created")));
        qputenv("XDG_DATA_HOME", previousDataHome);
    }

    void testHistoryRetentionAndClearHistory()
    {
        PulseTelemetryController controller(QDBusConnection::systemBus());
        QCOMPARE(controller.historyRetentionSeconds(), 300);
        QVERIFY(!controller.historySamples().isEmpty());

        controller.setHistoryRetentionSeconds(900);
        QCOMPARE(controller.historyRetentionSeconds(), 900);
        QVERIFY(controller.lastActionStatus().contains(QStringLiteral("15m")));

        controller.setHistoryRetentionSeconds(5000);
        QCOMPARE(controller.historyRetentionSeconds(), 1800);

        controller.clearHistory();
        QCOMPARE(controller.historySamples().size(), 0);
        QCOMPARE(controller.incidentHistory().size(), 0);
        QVERIFY(controller.lastActionSucceeded());
        QVERIFY(controller.lastActionStatus().contains(QStringLiteral("Cleared")));
    }

    void testCollectorConnectedAndReconnect()
    {
        PulseTelemetryController localController(QDBusConnection::systemBus());
        QVERIFY(localController.collectorConnected());

        PulseTelemetryController remoteController;
        // In local unit test without collector daemon running, remote defaults to disconnected/connecting
        QVERIFY(!remoteController.collectorConnected());
        remoteController.reconnectCollector();
        QCOMPARE(remoteController.collectorState(), QStringLiteral("Connecting"));
    }

    void testSystemResumeResetsBaselinesAndSetsGap()
    {
        PulseTelemetryController controller(QDBusConnection::systemBus());
        controller.refreshNow();
        controller.handleSystemResume();
        controller.refreshNow();
        QVERIFY(controller.historySamples().last().toMap().value(QStringLiteral("gapBefore")).toBool());
    }

    void testTemperatureUnitPreference()
    {
        PulseTelemetryController controller(QDBusConnection::systemBus());
        QCOMPARE(controller.temperatureUnit(), QStringLiteral("C"));
        QSignalSpy spy(&controller, &PulseTelemetryController::temperatureUnitChanged);
        controller.setTemperatureUnit(QStringLiteral("f"));
        QCOMPARE(controller.temperatureUnit(), QStringLiteral("F"));
        QCOMPARE(spy.count(), 1);
        controller.setTemperatureUnit(QStringLiteral("F"));
        QCOMPARE(spy.count(), 1);
        controller.setTemperatureUnit(QStringLiteral("Celsius"));
        QCOMPARE(controller.temperatureUnit(), QStringLiteral("C"));
        QCOMPARE(spy.count(), 2);
    }

    void testCollectorPreferencesUpdate()
    {
        PulseTelemetryController controller(QDBusConnection::systemBus());
        QVERIFY(controller.collectorPreferences().isEmpty());
        QSignalSpy spy(&controller, &PulseTelemetryController::collectorPreferencesChanged);
        QVariantMap prefs{{QStringLiteral("minNotificationSeverity"), 3}, {QStringLiteral("quietHoursEnabled"), true}};
        controller.updateCollectorPreferences(prefs);
        QCOMPARE(controller.collectorPreferences(), prefs);
        QCOMPARE(spy.count(), 1);
    }

    void testTrendsPropertiesAndRanges()
    {
        PulseTelemetryController controller(QDBusConnection::systemBus());
        QCOMPARE(controller.trendsRange(), QStringLiteral("1h"));
        QVERIFY(!controller.trendsLoading());
        QVERIFY(controller.trendsData().isEmpty());
        QVERIFY(controller.persistedIncidents().isEmpty());

        QSignalSpy rangeSpy(&controller, &PulseTelemetryController::trendsRangeChanged);
        controller.setTrendsRange(QStringLiteral("24h"));
        QCOMPARE(controller.trendsRange(), QStringLiteral("24h"));
        QCOMPARE(rangeSpy.count(), 1);
    }

    void testTrendsErrorsOrderingAndIncidentIntegration()
    {
        auto bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("pulse-trends-fixture"));
        QVERIFY(bus.isConnected());
        FakeTrendsCollector fake;
        QVERIFY(bus.registerService(QString::fromLatin1(CollectorServiceName)));
        QVERIFY(bus.registerObject(QString::fromLatin1(CollectorObjectPath), &fake, QDBusConnection::ExportAllSlots));
        PulseTelemetryController controller(QDBusConnection::systemBus());
        controller.requestTrends(100, 200, 120, {});
        controller.requestTrends(300, 400, 120, {});
        QTRY_VERIFY(!controller.trendsLoading());
        QCOMPARE(controller.trendsData().value(QStringLiteral("fromMs")).toLongLong(), 300LL);
        QTest::qWait(200);
        QCOMPARE(controller.trendsData().value(QStringLiteral("fromMs")).toLongLong(), 300LL);
        for (const auto &metric : HistoryStore::metrics()) QVERIFY(fake.requestedMetrics.contains(metric.id));

        fake.failNext = true;
        controller.requestTrends(500, 600, 120, {});
        QTRY_VERIFY(!controller.trendsLoading());
        QVERIFY(controller.trendsError().contains(QStringLiteral("Storage unavailable")));
        QVERIFY(controller.trendsData().isEmpty());

        controller.loadTrendsRange(QStringLiteral("24h"));
        QTRY_VERIFY(!controller.trendsLoading() && !controller.persistedIncidentsLoading());
        QVERIFY(controller.trendsError().isEmpty());
        QVERIFY(controller.persistedIncidentsError().isEmpty());
        QCOMPARE(controller.persistedIncidents().size(), 1);
        QCOMPARE(fake.trendsFrom, fake.incidentsFrom);
        QCOMPARE(fake.trendsTo, fake.incidentsTo);
        QCOMPARE(fake.trendsTo - fake.trendsFrom, 24LL * 3600000LL);

        controller.requestTrends(100, 200, 120, {});
        controller.clearPersistedHistory();
        controller.loadTrendsRange(QStringLiteral("6h"));
        QTRY_VERIFY(!controller.trendsLoading() && !controller.persistedIncidentsLoading());
        const auto postClearFrom = controller.trendsData().value(QStringLiteral("fromMs"));
        QVERIFY(postClearFrom.toLongLong() > 1000);
        QTest::qWait(220);
        QCOMPARE(controller.trendsData().value(QStringLiteral("fromMs")), postClearFrom);
        QCOMPARE(controller.persistedIncidents().size(), 1);

        fake.dropNext = true;
        controller.requestTrends(700, 800, 120, {});
        QTRY_VERIFY_WITH_TIMEOUT(!controller.trendsLoading(), 12000);
        QVERIFY(!controller.trendsError().isEmpty());

        bus.unregisterObject(QString::fromLatin1(CollectorObjectPath));
        controller.requestTrends(900, 1000, 120, {});
        QTRY_VERIFY_WITH_TIMEOUT(!controller.trendsLoading(), 3000);
        QVERIFY(!controller.trendsError().isEmpty());
        bus.unregisterService(QString::fromLatin1(CollectorServiceName));
    }

    void testProcessInspectionHasNoMutationApi()
    {
        PulseTelemetryController controller(QDBusConnection::systemBus());
        const auto *meta = controller.metaObject();
        for (int i = 0; i < meta->methodCount(); ++i) {
            const auto name = meta->method(i).name();
            QVERIFY(name != "terminateProcess");
            QVERIFY(name != "canTerminateProcess");
        }
    }

    void testProcessIoStats()
    {
        PulseTelemetryController controller(QDBusConnection::systemBus());

        // Invalid PID
        const auto invalidStats = controller.processIoStats(-1);
        QVERIFY(!invalidStats.value(QStringLiteral("available")).toBool());

        // Current process
        const auto myStats = controller.processIoStats(static_cast<int>(::getpid()));
        QVERIFY(myStats.value(QStringLiteral("available")).toBool());
        QVERIFY(myStats.contains(QStringLiteral("readBytes")));
        QVERIFY(myStats.contains(QStringLiteral("writeBytes")));
        QVERIFY(myStats.contains(QStringLiteral("readMib")));
        QVERIFY(myStats.contains(QStringLiteral("writeMib")));
    }
};

QTEST_MAIN(TestControllerActions)
#include "test_controller_actions.moc"
