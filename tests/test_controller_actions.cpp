#include "pulsetelemetrycontroller.h"
#include "collectorprotocol.h"
#include "historystore.h"
#include <QDBusContext>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QDir>
#include <QFile>
#include <QJSEngine>
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

class FakeMutablePowerProfiles : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.hadess.PowerProfiles")
    Q_PROPERTY(QString ActiveProfile READ activeProfile WRITE setActiveProfile)
    Q_PROPERTY(QStringList Profiles READ profiles)

public:
    explicit FakeMutablePowerProfiles(QObject *parent = nullptr) : QObject(parent) {}
    QString activeProfile() const { return m_activeProfile; }
    void setActiveProfile(const QString &profile) { m_activeProfile = profile; }
    QStringList profiles() const { return {QStringLiteral("balanced"), QStringLiteral("power-saver")}; }

private:
    QString m_activeProfile = QStringLiteral("balanced");
};

class FakeNetworkManager : public QDBusVirtualObject
{
    Q_OBJECT

public:
    explicit FakeNetworkManager(QObject *parent = nullptr) : QDBusVirtualObject(parent) {}

    QString introspect(const QString &) const override
    {
        return QStringLiteral("<node><interface name='org.freedesktop.DBus.Properties'/></node>");
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.interface() != QStringLiteral("org.freedesktop.DBus.Properties")) return false;
        const QString path = message.path();
        const QVariantList args = message.arguments();
        QVariantMap accessPoint;
        int delayMs = 1;

        if (message.member() == QStringLiteral("Get") && args.size() >= 2) {
            const QString property = args.at(1).toString();
            QVariant value;
            if (path == QStringLiteral("/org/freedesktop/NetworkManager")
                && property == QStringLiteral("Devices")) {
                value = QVariant::fromValue(QList<QDBusObjectPath>{
                    QDBusObjectPath(QStringLiteral("/org/freedesktop/NetworkManager/Devices/0")),
                    QDBusObjectPath(QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"))});
            } else if (path.endsWith(QStringLiteral("/Devices/0"))) {
                if (property == QStringLiteral("ActiveAccessPoint"))
                    value = QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/freedesktop/NetworkManager/AccessPoints/0")));
                else if (property == QStringLiteral("Interface")) {
                    value = QStringLiteral("wlan0");
                    delayMs = 90;
                } else if (property == QStringLiteral("Bitrate")) {
                    value = 54000U;
                    delayMs = 10;
                }
            } else if (path.endsWith(QStringLiteral("/Devices/1"))) {
                if (property == QStringLiteral("ActiveAccessPoint"))
                    value = QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/freedesktop/NetworkManager/AccessPoints/1")));
                else if (property == QStringLiteral("Interface")) {
                    value = QStringLiteral("wlan1");
                    delayMs = 5;
                } else if (property == QStringLiteral("Bitrate")) {
                    value = 433000U;
                    delayMs = 70;
                }
            }
            if (!value.isValid()) return false;
            const QDBusMessage reply = message.createReply({QVariant::fromValue(QDBusVariant(value))});
            QTimer::singleShot(delayMs, this, [connection, reply] { connection.send(reply); });
            return true;
        }

        if (message.member() == QStringLiteral("GetAll") && args.size() == 1) {
            if (path.endsWith(QStringLiteral("/AccessPoints/0"))) {
                accessPoint = {{QStringLiteral("Ssid"), QByteArrayLiteral("zero-signal")},
                    {QStringLiteral("Strength"), QVariant::fromValue<uchar>(0)},
                    {QStringLiteral("Frequency"), 2412U}};
                delayMs = 5;
            } else if (path.endsWith(QStringLiteral("/AccessPoints/1"))) {
                accessPoint = {{QStringLiteral("Ssid"), QByteArrayLiteral("fast-link")},
                    {QStringLiteral("Strength"), QVariant::fromValue<uchar>(73)},
                    {QStringLiteral("Frequency"), 5180U}};
                delayMs = 100;
            } else {
                return false;
            }
            const QDBusMessage reply = message.createReply({QVariant::fromValue(accessPoint)});
            QTimer::singleShot(delayMs, this, [connection, reply] { connection.send(reply); });
            return true;
        }
        return false;
    }
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
    void testWifiSignalVisibilityHonorsAvailabilityAtZero()
    {
        QFile source(QStringLiteral(PULSE_QML_SOURCE_DIR "/MeasurementLabels.js"));
        QVERIFY(source.open(QIODevice::ReadOnly | QIODevice::Text));
        QString code = QString::fromUtf8(source.readAll());
        const qsizetype pragmaEnd = code.indexOf(QLatin1Char('\n'));
        QVERIFY(pragmaEnd >= 0);
        code.remove(0, pragmaEnd + 1);

        QJSEngine engine;
        QVERIFY(!engine.evaluate(code).isError());
        const QJSValue visible = engine.globalObject().property(QStringLiteral("wifiSignalVisible"));
        QVERIFY(visible.isCallable());
        const auto evaluate = [&engine, &visible](const QVariantMap &row) {
            return visible.call({engine.toScriptValue(row)}).toBool();
        };
        QVERIFY(evaluate({{QStringLiteral("isWireless"), true},
            {QStringLiteral("wirelessSignalAvailable"), true}, {QStringLiteral("wirelessSignalPercent"), 0}}));
        QVERIFY(!evaluate({{QStringLiteral("isWireless"), true},
            {QStringLiteral("wirelessSignalAvailable"), false}, {QStringLiteral("wirelessSignalPercent"), 65}}));
        QVERIFY(evaluate({{QStringLiteral("isWireless"), true}, {QStringLiteral("wirelessSignalPercent"), 65}}));
        QVERIFY(!evaluate({{QStringLiteral("isWireless"), false},
            {QStringLiteral("wirelessSignalAvailable"), true}, {QStringLiteral("wirelessSignalPercent"), 0}}));
    }

    void testWifiTelemetryRemainsAssociatedWithEachAdapter()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FakeNetworkManager fake;
        const QString service = QStringLiteral("org.freedesktop.NetworkManager");
        const QString objectPath = QStringLiteral("/org/freedesktop/NetworkManager");
        QVERIFY(bus.registerService(service));
        QVERIFY(bus.registerVirtualObject(objectPath, &fake, QDBusConnection::SubPath));

        QTemporaryDir fixture;
        QVERIFY(fixture.isValid());
        const QString sysRoot = fixture.filePath(QStringLiteral("sys"));
        const QString procRoot = fixture.filePath(QStringLiteral("proc"));
        for (const QString &name : {QStringLiteral("wlan0"), QStringLiteral("wlan1")}) {
            QVERIFY(QDir().mkpath(QDir(sysRoot).filePath(QStringLiteral("class/net/") + name + QStringLiteral("/wireless"))));
            QFile ifindex(QDir(sysRoot).filePath(QStringLiteral("class/net/") + name + QStringLiteral("/ifindex")));
            QVERIFY(ifindex.open(QIODevice::WriteOnly | QIODevice::Text));
            QCOMPARE(ifindex.write(name == QStringLiteral("wlan0") ? "3\n" : "4\n"), qint64(2));
            ifindex.close();
        }
        QVERIFY(QDir().mkpath(QDir(procRoot).filePath(QStringLiteral("net"))));
        QFile netDev(QDir(procRoot).filePath(QStringLiteral("net/dev")));
        QVERIFY(netDev.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray netDevContents = "Inter-| Receive | Transmit\n face |bytes packets errs drop fifo frame compressed multicast|bytes packets errs drop fifo colls carrier compressed\n wlan0: 1000 0 0 0 0 0 0 0 2000 0 0 0 0 0 0 0\n wlan1: 3000 0 0 0 0 0 0 0 4000 0 0 0 0 0 0 0\n";
        QCOMPARE(netDev.write(netDevContents), static_cast<qint64>(netDevContents.size()));
        netDev.close();

        const QByteArray oldSysRoot = qgetenv("LOOFI_PULSE_SYS_ROOT");
        const QByteArray oldProcRoot = qgetenv("LOOFI_PULSE_PROC_ROOT");
        qputenv("LOOFI_PULSE_SYS_ROOT", QFile::encodeName(sysRoot));
        qputenv("LOOFI_PULSE_PROC_ROOT", QFile::encodeName(procRoot));
        {
            PulseTelemetryController controller(bus);
            const auto rowFor = [&controller](const QString &name) {
                for (const QVariant &item : controller.networkInterfaces()) {
                    const QVariantMap row = item.toMap();
                    if (row.value(QStringLiteral("name")).toString() == name) return row;
                }
                return QVariantMap{};
            };
            QTRY_VERIFY_WITH_TIMEOUT(rowFor(QStringLiteral("wlan0")).value(QStringLiteral("wirelessSignalAvailable")).toBool(), 3000);
            QTRY_VERIFY_WITH_TIMEOUT(rowFor(QStringLiteral("wlan1")).value(QStringLiteral("wirelessSignalAvailable")).toBool(), 3000);

            const QVariantMap first = rowFor(QStringLiteral("wlan0"));
            QCOMPARE(first.value(QStringLiteral("wirelessSsid")).toString(), QStringLiteral("zero-signal"));
            QCOMPARE(first.value(QStringLiteral("wirelessSignalPercent")).toInt(), 0);
            QCOMPARE(first.value(QStringLiteral("wirelessFrequencyGhz")).toDouble(), 2.412);
            QCOMPARE(first.value(QStringLiteral("wirelessBitrateMbps")).toInt(), 54);

            const QVariantMap second = rowFor(QStringLiteral("wlan1"));
            QCOMPARE(second.value(QStringLiteral("wirelessSsid")).toString(), QStringLiteral("fast-link"));
            QCOMPARE(second.value(QStringLiteral("wirelessSignalPercent")).toInt(), 73);
            QCOMPARE(second.value(QStringLiteral("wirelessFrequencyGhz")).toDouble(), 5.18);
            QCOMPARE(second.value(QStringLiteral("wirelessBitrateMbps")).toInt(), 433);
        }
        if (oldSysRoot.isEmpty()) qunsetenv("LOOFI_PULSE_SYS_ROOT");
        else qputenv("LOOFI_PULSE_SYS_ROOT", oldSysRoot);
        if (oldProcRoot.isEmpty()) qunsetenv("LOOFI_PULSE_PROC_ROOT");
        else qputenv("LOOFI_PULSE_PROC_ROOT", oldProcRoot);
        bus.unregisterObject(objectPath);
        bus.unregisterService(service);
    }

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

    void testAsyncProfileSetUpdatesCurrentSampleImmediately()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FakeMutablePowerProfiles fake;
        QVERIFY(bus.registerService(QStringLiteral("net.hadess.PowerProfiles")));
        QVERIFY(bus.registerObject(QStringLiteral("/net/hadess/PowerProfiles"), &fake, QDBusConnection::ExportAllProperties));

        PulseTelemetryController controller(bus);
        QTRY_VERIFY_WITH_TIMEOUT(controller.powerProfileServiceAvailable(), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(controller.activePowerProfile(), QStringLiteral("balanced"), 2000);
        QVERIFY(controller.activePowerProfileAvailable());
        QVERIFY(controller.setPowerProfile(QStringLiteral("power-saver")));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.powerProfileChangePending(), 2000);
        QVERIFY(controller.lastActionSucceeded());
        QCOMPARE(controller.activePowerProfile(), QStringLiteral("power-saver"));
        QVERIFY(controller.activePowerProfileAvailable());

        controller.refreshNow();
        const QVariantMap latest = controller.historySamples().last().toMap();
        QCOMPARE(latest.value(QStringLiteral("activePowerProfile")).toString(), QStringLiteral("power-saver"));
        QVERIFY(latest.value(QStringLiteral("metricQuality")).toMap().value(QStringLiteral("powerProfileAvailable")).toBool());

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
        QTRY_VERIFY_WITH_TIMEOUT(!controller.actionPending(), 5000);
        QVERIFY(!controller.lastActionSucceeded());
        QVERIFY(controller.lastActionStatus().contains(QStringLiteral("could not be created")));
        qputenv("XDG_DATA_HOME", previousDataHome);
    }

    void testIncidentKernelLogLoadingDoesNotBlock()
    {
        PulseTelemetryController controller(QDBusConnection::systemBus());
        SessionSnapshot marked;
        QString error;
        QVERIFY2(SessionStore::decode(controller.collectorMarkProblem(), &marked, &error), qPrintable(error));
        const QString markerId = marked.metadata.value(QStringLiteral("markerId")).toString();
        QVERIFY(!markerId.isEmpty());

        QElapsedTimer elapsed;
        elapsed.start();
        controller.loadIncidentKernelLogs(markerId);
        QVERIFY2(elapsed.elapsed() < 200, "Kernel journal retrieval must be scheduled away from the UI thread.");
        QTRY_VERIFY_WITH_TIMEOUT(!controller.incidentKernelLogsLoading(), 5000);
        QCOMPARE(controller.incidentKernelLogs(markerId), controller.incidentKernelLogEntries());
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

    void testHostInfoAndGroupedIncidents()
    {
        PulseTelemetryController controller(QDBusConnection::systemBus());
        QVERIFY(!controller.hostDistroName().isEmpty());
        QVERIFY(!controller.hostKernelVersion().isEmpty());
        QVERIFY(!controller.hostDeviceModel().isEmpty());

        // When incidentHistory is empty, groupedIncidentHistory must be empty
        QVERIFY(controller.groupedIncidentHistory().isEmpty());
    }
};

QTEST_MAIN(TestControllerActions)
#include "test_controller_actions.moc"
