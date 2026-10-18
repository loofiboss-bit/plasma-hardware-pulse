#include "pulsetelemetrycontroller.h"
#include "collectorprotocol.h"
#include <QtTest>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QElapsedTimer>
#include <QTimer>
#include <QEventLoop>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>
#include <time.h>
#include <unistd.h>
using namespace LoofiPulse;
class CollectorV09Test : public QObject {
    Q_OBJECT
    QTemporaryDir fixture;
    QProcess helper;
    void startHelper() {
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("LOOFI_PULSE_SYS_ROOT"), fixture.filePath(QStringLiteral("sys")));
        environment.insert(QStringLiteral("LOOFI_PULSE_PROC_ROOT"), fixture.filePath(QStringLiteral("proc")));
        helper.setProcessEnvironment(environment);
        helper.start(QStringLiteral(PULSE_SERVICE_BIN));
        QVERIFY(helper.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QDBusConnection::sessionBus().interface()->isServiceRegistered(QString::fromLatin1(CollectorServiceName)).value(), 5000);
    }
    QByteArray request(const QString &method, const QVariantList &arguments = {}) {
        auto message = QDBusMessage::createMethodCall(QString::fromLatin1(CollectorServiceName), QString::fromLatin1(CollectorObjectPath), QString::fromLatin1(CollectorInterface), method);
        message.setArguments(arguments);
        auto reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 5000);
        if (reply.type() == QDBusMessage::ErrorMessage) { qWarning() << reply.errorMessage(); return {}; }
        return reply.arguments().value(0).toByteArray();
    }
    static QVariantMap object(const QByteArray &data) { return QJsonDocument::fromJson(data).object().toVariantMap(); }
    static qint64 residentBytes() {
        QFile file(QStringLiteral("/proc/self/statm"));
        if (!file.open(QIODevice::ReadOnly)) return -1;
        const auto fields = file.readAll().simplified().split(' ');
        return fields.size() > 1 ? fields.at(1).toLongLong() * sysconf(_SC_PAGESIZE) : -1;
    }
    static double cpuSeconds() {
        timespec value{}; clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value);
        return double(value.tv_sec) + double(value.tv_nsec) / 1000000000.0;
    }
private Q_SLOTS:
    void initTestCase() {
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        QVERIFY(fixture.isValid());
        const auto put = [&](const QString &path, const QByteArray &contents) {
            QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(contents), contents.size());
        };
        put(fixture.filePath(QStringLiteral("proc/stat")), "cpu 1000 0 0 1000 0 0 0 0\ncpu0 1000 0 0 1000 0 0 0 0\n");
        for (int i = 1; i <= 1000; ++i) {
            QStringList fields(22, QStringLiteral("0")); fields[0] = QStringLiteral("S"); fields[11] = QString::number(i); fields[19] = QString::number(i * 100); fields[21] = QStringLiteral("128");
            put(fixture.filePath(QStringLiteral("proc/%1/stat").arg(i)), QStringLiteral("%1 (private-fixture-%1) %2\n").arg(i).arg(fields.join(QLatin1Char(' '))).toUtf8());
        }
        startHelper();
    }
    void init() {
        // Recover the isolated helper if a preceding restart test failed halfway through.
        if (helper.state() == QProcess::NotRunning) startHelper();
    }
    void cleanup() {
        if (helper.state() == QProcess::Running) {
            request(QStringLiteral("Unsubscribe"), {QStringLiteral("probe")});
            request(QStringLiteral("SetBackground"), {false});
            request(QStringLiteral("SetStartAtLogin"), {false});
        }
    }
    void sharedCollectionFrozenSessionAndPrivacy() {
        PulseTelemetryController app, widget;
        app.setPollIntervalMs(250); app.setHistoryRetentionSeconds(900);
        widget.setPollIntervalMs(1000); widget.setNotificationsEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(app.historySamples().size() >= 3, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(widget.historySamples().size() >= 3, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(app.allProcesses().size(), 1000, 10000);
        const auto packet = object(request(QStringLiteral("Subscribe"), {QStringLiteral("probe"), 1000, 300, false}));
        QVERIFY(!packet.value(QStringLiteral("captureId")).toString().isEmpty());
        QCOMPARE(packet.value(QStringLiteral("live")).toMap().value(QStringLiteral("effectivePollIntervalMs")).toInt(), 250);
        QCOMPARE(packet.value(QStringLiteral("live")).toMap().value(QStringLiteral("effectiveRetentionSeconds")).toInt(), 900);
        const auto first = app.historySamples().first().toMap();
        const auto widgetFirst = widget.historySamples().first().toMap();
        QCOMPARE(first.value(QStringLiteral("captureId")), widgetFirst.value(QStringLiteral("captureId")));
        app.markProblemNow();
        QTRY_VERIFY_WITH_TIMEOUT(app.sessionMode(), 5000);
        QVERIFY(app.unsavedInvestigation());
        const auto frozen = app.historySamples();
        QTest::qWait(650);
        QCOMPARE(app.historySamples(), frozen);
        QVERIFY(widget.historySamples().size() > frozen.size());
        const QString path = fixture.filePath(QStringLiteral("incident.pulse-session"));
        app.saveSession(path);
        QTRY_VERIFY_WITH_TIMEOUT(!app.actionPending(), 5000);
        QVERIFY2(app.lastActionSucceeded(), qPrintable(app.lastActionStatus()));
        QVERIFY(!app.unsavedInvestigation());
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto bytes = file.readAll();
        QVERIFY(!bytes.contains("private-fixture-"));
        SessionSnapshot saved; QString error;
        QVERIFY2(SessionStore::decode(bytes, &saved, &error), qPrintable(error));
        QCOMPARE(saved.samples.size(), frozen.size());
        const QString markerId = saved.metadata.value(QStringLiteral("markerId")).toString();
        QVERIFY2(!markerId.isEmpty(), "A manual problem marker must have a transferable identity.");
        SessionSnapshot handedOff;
        QVERIFY2(SessionStore::decode(request(QStringLiteral("Incident"), {markerId}), &handedOff, &error), qPrintable(error));
        QCOMPARE(handedOff.metadata.value(QStringLiteral("markerId")).toString(), markerId);
        QCOMPARE(handedOff.samples.size(), saved.samples.size());
        for (int i = 0; i < saved.samples.size(); ++i) {
            QCOMPARE(handedOff.samples.at(i).sampleId, saved.samples.at(i).sampleId);
            QCOMPARE(handedOff.samples.at(i).timestampMs, saved.samples.at(i).timestampMs);
            QCOMPARE(handedOff.samples.at(i).cpuLoadPercent, saved.samples.at(i).cpuLoadPercent);
        }
        const auto original = frozen.first().toMap();
        QCOMPARE(saved.samples.first().cpuLoadPercent, original.value(QStringLiteral("cpuLoad")).toDouble());
        app.returnToLive(); QVERIFY(!app.sessionMode());
        app.setSelectedHistorySampleId(app.historySamples().first().toMap().value(QStringLiteral("sampleId")).toULongLong());
        QVERIFY(app.selectedHistorySampleId() != 0);
        app.returnToLive();
        QCOMPARE(app.selectedHistorySampleId(), qulonglong(0));
        app.openSession(path); QTRY_VERIFY_WITH_TIMEOUT(!app.actionPending(), 5000);
        QVERIFY(app.sessionMode()); QCOMPARE(app.historySamples().size(), frozen.size());
        app.exportEntireRecording();
        QTRY_VERIFY_WITH_TIMEOUT(!app.actionPending(), 5000);
        QVERIFY2(app.lastActionSucceeded(), qPrintable(app.lastActionStatus()));
        QString jsonPath = app.lastActionPath();
        jsonPath.replace(QStringLiteral("loofi-pulse-report-"), QStringLiteral("loofi-pulse-samples-"));
        if (jsonPath.endsWith(QStringLiteral(".md"))) { jsonPath.chop(3); jsonPath.append(QStringLiteral(".json")); }
        QFile report(jsonPath); QVERIFY2(report.open(QIODevice::ReadOnly), qPrintable(jsonPath));
        const auto document = QJsonDocument::fromJson(report.readAll());
        QVERIFY2(document.isArray(), "Schema 2 reports must retain their existing JSON array format.");
        const auto exported = document.array();
        QCOMPARE(exported.size(), saved.samples.size());
        for (int i = 0; i < exported.size(); ++i) {
            const auto exportedSample = exported.at(i).toObject();
            QCOMPARE(exportedSample.value(QStringLiteral("schemaVersion")).toInt(), 2);
            QCOMPARE(exportedSample.value(QStringLiteral("sampleId")).toInteger(), qint64(saved.samples.at(i).sampleId));
            QCOMPARE(exportedSample.value(QStringLiteral("timestampMs")).toInteger(), saved.samples.at(i).timestampMs);
        }
        const auto intact = app.historySamples();
        const QString bad = fixture.filePath(QStringLiteral("broken.pulse-session")); QFile invalid(bad); QVERIFY(invalid.open(QIODevice::WriteOnly)); invalid.write("{bad"); invalid.close();
        app.openSession(bad); QTRY_VERIFY_WITH_TIMEOUT(!app.actionPending(), 5000);
        QVERIFY(!app.lastActionSucceeded()); QCOMPARE(app.historySamples(), intact);
        app.returnToLive(); widget.clearHistory();
        QTRY_VERIFY_WITH_TIMEOUT(app.historySamples().isEmpty() || app.historySamples().first().toMap().value(QStringLiteral("captureId")) != first.value(QStringLiteral("captureId")), 5000);
        request(QStringLiteral("Unsubscribe"), {QStringLiteral("probe")});
    }
    void importedMetadataIsRedactedFromDefaultReports() {
        SessionSnapshot imported;
        imported.recordingId = QStringLiteral("imported-original-capture");
        const QString privateName = QStringLiteral("private-imported-command");
        const int privatePid = 1867530997;
        for (int i = 0; i < 2; ++i) {
            FlightSample sample;
            sample.sampleId = quint64(i + 1); sample.monotonicMs = 1000 + i * 1900000;
            sample.timestampMs = 1700000000000LL + i * 1900000;
            sample.culpritName = privateName; sample.culpritPid = privatePid;
            sample.verdictHeadline = QStringLiteral("CPU pressure observed");
            sample.verdictEvidence = {QStringLiteral("CPU pressure remained elevated"), privateName};
            sample.metricQuality = {{QStringLiteral("cpuLoadAvailable"), true}};
            imported.samples.append(sample);
        }
        imported.metadata = {
            {QStringLiteral("selectionStartSampleId"), 1}, {QStringLiteral("selectionEndSampleId"), 2},
            {QStringLiteral("processName"), privateName}, {QStringLiteral("pid"), privatePid},
            {QStringLiteral("notes"), QVariantMap{{QStringLiteral("commandLine"), privateName}, {QStringLiteral("privateLabel"), QStringLiteral("private-metadata-only")}}},
            {QStringLiteral("comparison"), QVariantMap{{QStringLiteral("before"), QVariantMap{
                {QStringLiteral("available"), true}, {QStringLiteral("sampleCount"), 2},
                {QStringLiteral("cpuLoadPercent"), 14.0}, {QStringLiteral("pid"), privatePid},
                {QStringLiteral("note"), privateName}}}}}};
        const QString source = fixture.filePath(QStringLiteral("imported-private.pulse-session"));
        QString error;
        QVERIFY2(SessionStore::write(source, imported, true, &error), qPrintable(error));
        PulseTelemetryController client;
        client.setExportIncludeProcessDetails(false);
        client.openSession(source);
        QTRY_VERIFY_WITH_TIMEOUT(!client.actionPending(), 5000);
        QVERIFY2(client.lastActionSucceeded(), qPrintable(client.lastActionStatus()));
        QVERIFY(client.sessionMode());
        const auto frozen = client.historySamples();
        QCOMPARE(frozen.size(), 2);
        client.setHistoryRetentionSeconds(1800);
        auto settings = object(request(QStringLiteral("Subscribe"), {QStringLiteral("probe"), 1000, 300, false}));
        QCOMPARE(settings.value(QStringLiteral("live")).toMap().value(QStringLiteral("effectiveRetentionSeconds")).toInt(), 1800);
        client.setHistoryRetentionSeconds(300);
        QCOMPARE(client.historySamples(), frozen);
        client.exportSelectedIncident();
        QTRY_VERIFY_WITH_TIMEOUT(!client.actionPending(), 5000);
        QVERIFY2(client.lastActionSucceeded(), qPrintable(client.lastActionStatus()));
        QString reportPath = client.lastActionPath();
        reportPath.replace(QStringLiteral("loofi-pulse-report-"), QStringLiteral("loofi-pulse-samples-"));
        if (reportPath.endsWith(QStringLiteral(".md"))) { reportPath.chop(3); reportPath.append(QStringLiteral(".json")); }
        QFile report(reportPath);
        QVERIFY2(report.open(QIODevice::ReadOnly), qPrintable(reportPath));
        const auto bytes = report.readAll();
        QVERIFY(!bytes.contains(privateName.toUtf8()));
        QVERIFY(!bytes.contains(QByteArray::number(privatePid)));
        QVERIFY(!bytes.contains("private-metadata-only"));
        QVERIFY(!bytes.contains("commandLine"));
        QVERIFY(bytes.contains("CPU pressure remained elevated"));
        const auto samples = QJsonDocument::fromJson(bytes).array();
        QCOMPARE(samples.size(), 2);
        const auto selection = samples.first().toObject().value(QStringLiteral("selection")).toObject();
        QCOMPARE(selection.value(QStringLiteral("selectionStartSampleId")).toVariant().toULongLong(), quint64(1));
        QCOMPARE(selection.value(QStringLiteral("selectionEndSampleId")).toVariant().toULongLong(), quint64(2));
        QVERIFY(!selection.contains(QStringLiteral("processName")));
        const auto before = selection.value(QStringLiteral("comparison")).toObject().value(QStringLiteral("before")).toObject();
        QVERIFY(before.value(QStringLiteral("available")).toBool());
        QCOMPARE(before.value(QStringLiteral("sampleCount")).toInt(), 2);
        QVERIFY(!before.contains(QStringLiteral("pid")));
        QVERIFY(!before.contains(QStringLiteral("note")));
    }
    void rejectsOldCaptureSampleThroughActualReceiveSlot() {
        PulseTelemetryController client;
        client.setPollIntervalMs(250);
        QTRY_VERIFY_WITH_TIMEOUT(client.historySamples().size() >= 2, 10000);
        const auto before = client.historySamples().last().toMap();
        const auto realPacket = object(request(QStringLiteral("Subscribe"), {QStringLiteral("probe"), 250, 300, false}));
        const QString capture = realPacket.value(QStringLiteral("captureId")).toString();
        QVERIFY(!capture.isEmpty());
        auto oldSample = realPacket.value(QStringLiteral("sample")).toMap();
        QVERIFY(!oldSample.isEmpty());
        oldSample[QStringLiteral("captureId")] = QStringLiteral("retired-capture-delayed-frame");
        const quint64 poisonedId = before.value(QStringLiteral("sampleId")).toULongLong() + 1000000;
        oldSample[QStringLiteral("sampleId")] = QString::number(poisonedId);
        // Feed a delayed old-capture sample through the production private receiver,
        // with the current outer capture as when a queued sample is replayed after reconnect.
        const QVariantMap delayed{{QStringLiteral("captureId"), capture},
            {QStringLiteral("live"), realPacket.value(QStringLiteral("live"))},
            {QStringLiteral("incidents"), realPacket.value(QStringLiteral("incidents"))},
            {QStringLiteral("sample"), oldSample}};
        const auto bytes = QJsonDocument(QJsonObject::fromVariantMap(delayed)).toJson(QJsonDocument::Compact);
        QVERIFY(QMetaObject::invokeMethod(&client, "receiveCollectorFrame", Qt::DirectConnection, Q_ARG(QByteArray, bytes)));
        QVERIFY(client.historySamples().last().toMap().value(QStringLiteral("sampleId")).toULongLong() != poisonedId);
        const quint64 previousId = before.value(QStringLiteral("sampleId")).toULongLong();
        QTRY_VERIFY_WITH_TIMEOUT(client.historySamples().last().toMap().value(QStringLiteral("sampleId")).toULongLong() >= previousId + 2, 5000);
        for (const auto &value : client.historySamples())
            QCOMPARE(value.toMap().value(QStringLiteral("captureId")).toString(), capture);
    }
    void restartReconnectAndPreferences() {
        PulseTelemetryController client; client.setPollIntervalMs(250);
        QTRY_VERIFY_WITH_TIMEOUT(client.historySamples().size() >= 2, 10000);
        const auto capture = client.historySamples().last().toMap().value(QStringLiteral("captureId"));
        client.setBackgroundMonitoring(true);
        QTRY_VERIFY_WITH_TIMEOUT(client.backgroundMonitoring(), 5000);
        client.setStartAtLogin(true);
        QTRY_VERIFY_WITH_TIMEOUT(client.startAtLogin(), 5000);
        client.setBackgroundMonitoring(false);
        QTRY_VERIFY_WITH_TIMEOUT(!client.backgroundMonitoring(), 5000);
        QVERIFY(client.startAtLogin());
        client.setStartAtLogin(false); QTRY_VERIFY_WITH_TIMEOUT(!client.startAtLogin(), 5000);
        helper.terminate(); QVERIFY(helper.waitForFinished(5000));
        startHelper();
        QTRY_VERIFY_WITH_TIMEOUT(!client.historySamples().isEmpty() && client.historySamples().last().toMap().value(QStringLiteral("captureId")) != capture, 10000);
        QVERIFY(client.collectorState().startsWith(QStringLiteral("Live")));
    }
    void importedIncidentAndEmptyLoadPreserveOriginalSource() {
        SessionSnapshot original;
        original.recordingId = QStringLiteral("historical-recording");
        original.engineVersion = QStringLiteral("historical-engine");
        for (int i = 0; i < 2; ++i) {
            FlightSample sample;
            sample.sampleId = quint64(i + 1); sample.monotonicMs = 1000 + i * 1900000;
            sample.timestampMs = 1700000000000LL + i * 1900000;
            original.samples.append(sample);
        }
        const QString incidentId = QStringLiteral("original-incident-identity");
        original.incidents = {QVariantMap{{QStringLiteral("incidentId"), incidentId},
            {QStringLiteral("startSampleId"), 1}, {QStringLiteral("endSampleId"), 2},
            {QStringLiteral("sampleId"), 2}, {QStringLiteral("startMs"), 1000}, {QStringLiteral("endMs"), 1901000}}};
        const QString source = fixture.filePath(QStringLiteral("original-wide-session.pulse-session"));
        QString error;
        QVERIFY2(SessionStore::write(source, original, true, &error), qPrintable(error));
        PulseTelemetryController client;
        client.openSession(source); QTRY_VERIFY_WITH_TIMEOUT(!client.actionPending(), 5000);
        QVERIFY2(client.lastActionSucceeded(), qPrintable(client.lastActionStatus()));
        const auto frozen = client.historySamples(); QCOMPARE(frozen.size(), 2);
        auto empty = QJsonDocument::fromJson(SessionStore::encode(original, true)).object();
        empty[QStringLiteral("samples")] = QJsonArray{};
        const QString emptyPath = fixture.filePath(QStringLiteral("empty-session.pulse-session"));
        QFile emptyFile(emptyPath); QVERIFY(emptyFile.open(QIODevice::WriteOnly));
        const auto bytes = QJsonDocument(empty).toJson(); QCOMPARE(emptyFile.write(bytes), bytes.size()); emptyFile.close();
        client.openSession(emptyPath); QTRY_VERIFY_WITH_TIMEOUT(!client.actionPending(), 5000);
        QVERIFY(!client.lastActionSucceeded()); QCOMPARE(client.historySamples(), frozen);
        client.exportEntireRecording(); QTRY_VERIFY_WITH_TIMEOUT(!client.actionPending(), 5000);
        QVERIFY2(client.lastActionSucceeded(), qPrintable(client.lastActionStatus()));
        QString reportPath = client.lastActionPath();
        reportPath.replace(QStringLiteral("loofi-pulse-report-"), QStringLiteral("loofi-pulse-samples-"));
        if (reportPath.endsWith(QStringLiteral(".md"))) { reportPath.chop(3); reportPath += QStringLiteral(".json"); }
        QFile report(reportPath); QVERIFY(report.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(report.readAll()).array().size(), 2);
        client.selectIncident(incidentId);
        QCOMPARE(client.historySamples().size(), 2);
        const QString selectedPath = fixture.filePath(QStringLiteral("selected-original.pulse-session"));
        client.saveSession(selectedPath); QTRY_VERIFY_WITH_TIMEOUT(!client.actionPending(), 5000);
        QVERIFY2(client.lastActionSucceeded(), qPrintable(client.lastActionStatus()));
        SessionSnapshot selected; QVERIFY2(SessionStore::read(selectedPath, &selected, &error), qPrintable(error));
        QCOMPARE(selected.recordingId, original.recordingId);
        QCOMPARE(selected.engineVersion, original.engineVersion);
        QCOMPARE(selected.metadata.value(QStringLiteral("incidentId")).toString(), incidentId);
        QCOMPARE(selected.samples.size(), 2);
    }
    void actualQmlProcessPageResponsiveness() {
        QQmlApplicationEngine engine;
        QStringList runtimeErrors;
        connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings) {
                const QString text = warning.toString();
                if (text.contains(QStringLiteral("TypeError")) || text.contains(QStringLiteral("ReferenceError"))
                    || text.contains(QStringLiteral("Binding loop")) || text.contains(QStringLiteral("Unable to assign")))
                    runtimeErrors.append(text);
            }
        });
        engine.addImportPath(QStringLiteral(PULSE_BUILD_BIN));
        engine.addImportPath(QStringLiteral(PULSE_APP_QML));
        engine.load(QUrl::fromLocalFile(QStringLiteral(PULSE_APP_QML "/Main.qml")));
        QVERIFY2(!engine.rootObjects().isEmpty(), "The production application QML must load.");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        auto *controller = window->findChild<QObject *>(QStringLiteral("telemetryController")); QVERIFY(controller);
        controller->setProperty("pollIntervalMs", 250);
        controller->setProperty("historyRetentionSeconds", 1800);
        window->resize(1000, 780); window->show();
        QVERIFY(QMetaObject::invokeMethod(window, "switchToPage", Q_ARG(QVariant, QStringLiteral("processes")), Q_ARG(QVariant, QString())));
        QTRY_COMPARE_WITH_TIMEOUT(controller->property("allProcesses").toList().size(), 1000, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->property("historySamples").toList().isEmpty(), 5000);
        auto *processPage = window->findChild<QQuickItem *>(QStringLiteral("processesPage")); QVERIFY(processPage);
        auto *processList = processPage->findChild<QQuickItem *>(QStringLiteral("processList")); QVERIFY(processList);
        QTRY_COMPARE_WITH_TIMEOUT(processList->property("count").toInt(), 1000, 5000);
        const bool prefilledHistory = qEnvironmentVariableIntValue("PULSE_PREFILL_HISTORY") == 1;
        FlightSample syntheticTail;
        const auto injectFrame = [controller](const FlightSample &sample) {
            const QVariantMap packet{{QStringLiteral("captureId"), sample.captureId},
                {QStringLiteral("sample"), SessionStore::sampleToMap(sample)}, {QStringLiteral("incidents"), QVariantList{}}};
            const auto bytes = QJsonDocument(QJsonObject::fromVariantMap(packet)).toJson(QJsonDocument::Compact);
            return QMetaObject::invokeMethod(controller, "receiveCollectorFrame", Qt::DirectConnection, Q_ARG(QByteArray, bytes));
        };
        if (prefilledHistory) {
            FlightSample base;
            const auto original = controller->property("historySamples").toList().last().toMap();
            QVERIFY(SessionStore::sampleFromMap(original, &base));
            QSignalBlocker blocker(controller);
            for (int i = 1; i <= 7201; ++i) {
                auto sample = base;
                sample.sampleId = base.sampleId + quint64(i);
                sample.monotonicMs = base.monotonicMs + qint64(i) * 250;
                sample.timestampMs = base.timestampMs + qint64(i) * 250;
                sample.elapsedSincePreviousMs = 250;
                sample.cpuLoadPercent = i % 101; sample.cpuTempCelsius = 25 + i % 76;
                sample.responsivenessScore = 100 - i % 101;
                sample.metricQuality[QStringLiteral("cpuLoadAvailable")] = true;
                sample.metricQuality[QStringLiteral("cpuTempAvailable")] = true;
                sample.metricQuality[QStringLiteral("psiAvailable")] = true;
                sample.metricQuality[QStringLiteral("gapBefore")] = i % 1000 == 0;
                QVERIFY(injectFrame(sample));
                syntheticTail = sample;
            }
            QCOMPARE(controller->property("historySamples").toList().size(), 7201);
            QCOMPARE(controller->property("historySummary").toMap().value(QStringLiteral("count")).toInt(), 7201);
            QElapsedTimer projectionClock; projectionClock.start();
            QVariantMap projection;
            QVERIFY(QMetaObject::invokeMethod(controller, "historyPlot", Q_RETURN_ARG(QVariantMap, projection), Q_ARG(int, 64)));
            qInfo() << "Native 64-column plot projection took" << projectionClock.nsecsElapsed() / 1000000.0 << "ms";
            QVERIFY(!projection.isEmpty());
            for (auto it = projection.cbegin(); it != projection.cend(); ++it)
                QVERIFY(it.value().toList().size() <= (it.key() == QStringLiteral("markers") ? 64 : 256));
            bool preservesPeak = false;
            int gapCount = 0;
            for (const auto &point : projection.value(QStringLiteral("cpuLoad")).toList()) {
                const auto row = point.toMap();
                preservesPeak |= row.value(QStringLiteral("value")).toDouble() == 100.0;
                gapCount += row.value(QStringLiteral("gapBefore")).toBool() ? 1 : 0;
            }
            QVERIFY(preservesPeak);
            QVERIFY(gapCount >= 8);
            int firstIndex = -1, lastIndex = -1;
            QVERIFY(QMetaObject::invokeMethod(controller, "historyIndexAtRatio", Q_RETURN_ARG(int, firstIndex), Q_ARG(double, 0.0)));
            QVERIFY(QMetaObject::invokeMethod(controller, "historyIndexAtRatio", Q_RETURN_ARG(int, lastIndex), Q_ARG(double, 1.0)));
            QCOMPARE(firstIndex, 0);
            QCOMPARE(lastIndex, 7200);
            QVERIFY(QMetaObject::invokeMethod(window, "switchToPage", Q_ARG(QVariant, QStringLiteral("flight")), Q_ARG(QVariant, QString())));
            qInfo() << "Prefilled presentation regression: synthetic 7,201-point history with peaks and gaps; real service frames continue invalidating the production UI. This is not a wall-clock retention soak.";
        }
        if (prefilledHistory) {
            auto *canvas = window->findChild<QQuickItem *>(QStringLiteral("section_timeline"));
            QVERIFY(canvas);
            qInfo() << "History canvas dimensions" << canvas->width() << canvas->height();
        }
        bool soakOk = false;
        const qint64 requestedSoak = qEnvironmentVariableIntValue("PULSE_SOAK_MS", &soakOk);
        const int soakMs = int(std::clamp<qint64>(soakOk ? requestedSoak : 5000, 5000, 3600000));
        const auto helperRss = [&]() {
            QFile file(QStringLiteral("/proc/%1/statm").arg(helper.processId()));
            if (!file.open(QIODevice::ReadOnly)) return qint64(-1);
            const auto fields = file.readAll().simplified().split(' ');
            return fields.size() > 1 ? fields.at(1).toLongLong() * sysconf(_SC_PAGESIZE) : qint64(-1);
        };
        const auto helperCpu = [&]() {
            QFile file(QStringLiteral("/proc/%1/stat").arg(helper.processId()));
            if (!file.open(QIODevice::ReadOnly)) return -1.0;
            const auto content = file.readAll();
            const auto fields = content.mid(content.lastIndexOf(')') + 2).simplified().split(' ');
            return fields.size() > 12 ? double(fields.at(11).toULongLong() + fields.at(12).toULongLong()) / double(sysconf(_SC_CLK_TCK)) : -1.0;
        };
        const qint64 rssBefore = residentBytes(), serviceRssBefore = helperRss();
        const double cpuBefore = cpuSeconds(), serviceCpuBefore = helperCpu();
        QElapsedTimer clock; clock.start();
        // Fixed-size histograms avoid making the measurement itself grow throughout a long soak.
        // Each bucket is an upper latency bound in 0.1 ms steps; the final bucket means >=1000 ms.
        QList<quint64> totalHistogram(10001, 0), windowHistogram(10001, 0);
        quint64 heartbeatCount = 0, windowCount = 0;
        const auto percentile95 = [](const QList<quint64> &histogram, quint64 count) {
            const quint64 target = (count * 95 + 99) / 100;
            quint64 seen = 0;
            for (int i = 0; i < histogram.size(); ++i) { seen += histogram.at(i); if (seen >= target) return double(i) / 10.0; }
            return 1000.0;
        };
        qint64 previous = 0, windowStartMs = 0;
        int lastMinute = 0;
        qint64 peakUiRss = rssBefore, peakServiceRss = serviceRssBefore;
        qint64 fullWindowUiRss = -1, fullWindowServiceRss = -1;
        bool historySwitched = prefilledHistory;
        QList<double> windowP95;
        QEventLoop loop;
        QTimer syntheticFrames;
        syntheticFrames.setTimerType(Qt::PreciseTimer); syntheticFrames.setInterval(250);
        if (prefilledHistory) connect(&syntheticFrames, &QTimer::timeout, &loop, [&] {
            ++syntheticTail.sampleId; syntheticTail.monotonicMs += 250; syntheticTail.timestampMs += 250;
            syntheticTail.cpuLoadPercent = syntheticTail.sampleId % 101;
            syntheticTail.cpuTempCelsius = 25 + syntheticTail.sampleId % 76;
            syntheticTail.responsivenessScore = 100 - syntheticTail.sampleId % 101;
            syntheticTail.metricQuality[QStringLiteral("gapBefore")] = syntheticTail.sampleId % 1000 == 0;
            if (!injectFrame(syntheticTail)) runtimeErrors.append(QStringLiteral("Synthetic rolling history frame could not be delivered."));
        });
        QTimer heartbeat;
        heartbeat.setTimerType(Qt::PreciseTimer); heartbeat.setInterval(5);
        connect(&heartbeat, &QTimer::timeout, &loop, [&] {
            const qint64 now = clock.nsecsElapsed();
            if (previous) {
                const double delay = std::max(0.0, double(now - previous) / 1000000.0 - 5.0);
                const int bucket = int(std::clamp(std::ceil(delay * 10.0), 0.0, 10000.0));
                ++totalHistogram[bucket]; ++windowHistogram[bucket]; ++heartbeatCount; ++windowCount;
            }
            previous = now;
            if (!historySwitched && heartbeatCount % 40 == 0) {
                const double maximum = std::max(0.0, processList->property("contentHeight").toDouble() - processList->height());
                if (maximum > 0) processList->setProperty("contentY", std::fmod(double(heartbeatCount) * 10.0, maximum));
            }
        });
        const auto flushWindow = [&]() {
            if (!windowCount) return;
            const double p95 = percentile95(windowHistogram, windowCount);
            windowP95.append(p95);
            qInfo().noquote() << QStringLiteral("QML soak window: elapsed=%1..%2 s; page=%3; heartbeat count=%4, p95 upper bound=%5 ms; peak UI RSS=%6; peak helper RSS=%7 bytes")
                .arg(double(windowStartMs) / 1000.0, 0, 'f', 3).arg(double(clock.elapsed()) / 1000.0, 0, 'f', 3)
                .arg(historySwitched ? QStringLiteral("History") : QStringLiteral("Processes"))
                .arg(windowCount).arg(p95, 0, 'f', 1).arg(peakUiRss).arg(peakServiceRss);
            windowHistogram.fill(0); windowCount = 0; windowStartMs = clock.elapsed();
            peakUiRss = residentBytes(); peakServiceRss = helperRss();
        };
        QTimer progress;
        progress.setInterval(1000);
        connect(&progress, &QTimer::timeout, &loop, [&] {
            const qint64 elapsedMs = clock.elapsed();
            const qint64 uiRss = residentBytes(), serviceRss = helperRss();
            peakUiRss = std::max(peakUiRss, uiRss); peakServiceRss = std::max(peakServiceRss, serviceRss);
            const int minute = int(elapsedMs / 60000);
            if (minute > lastMinute) {
                lastMinute = minute;
                const auto samples = controller->property("historySamples").toList();
                qInfo().noquote() << QStringLiteral("QML soak progress: elapsed=%1 s; page=%2; retained samples=%3; latest sample ID=%4; UI RSS=%5; helper RSS=%6 bytes; UI CPU=%7 s; helper CPU=%8 s")
                    .arg(double(elapsedMs) / 1000.0, 0, 'f', 3).arg(historySwitched ? QStringLiteral("History") : QStringLiteral("Processes"))
                    .arg(samples.size()).arg(samples.isEmpty() ? QStringLiteral("none") : samples.last().toMap().value(QStringLiteral("sampleId")).toString())
                    .arg(uiRss).arg(serviceRss).arg(cpuSeconds() - cpuBefore, 0, 'f', 3).arg(helperCpu() - serviceCpuBefore, 0, 'f', 3);
                if (minute % 5 == 0) flushWindow();
                if (minute >= 30 && !historySwitched) {
                    historySwitched = QMetaObject::invokeMethod(window, "switchToPage", Q_ARG(QVariant, QStringLiteral("flight")), Q_ARG(QVariant, QString()));
                    if (!historySwitched) runtimeErrors.append(QStringLiteral("The production History page could not be opened after 30 minutes."));
                    qInfo() << "QML soak: switching to History after 30 minutes with" << samples.size() << "retained samples; 1,000-process collection continues.";
                }
                // Allow one minute for the full History page's one-time render allocations.
                if (minute >= 31 && fullWindowUiRss < 0) { fullWindowUiRss = uiRss; fullWindowServiceRss = serviceRss; }
            }
        });
        qInfo() << "QML soak starting: requested wall-clock duration" << soakMs << "ms; helper PID" << helper.processId() << "; process fixture1000; sample interval250ms; retention1800s.";
        QTimer::singleShot(soakMs, &loop, &QEventLoop::quit);
        heartbeat.start(); progress.start(); if (prefilledHistory) syntheticFrames.start();
        loop.exec(); heartbeat.stop(); progress.stop(); syntheticFrames.stop();
        flushWindow();
        const double elapsed = double(clock.nsecsElapsed()) / 1000000000.0;
        const double cpuUsed = cpuSeconds() - cpuBefore, serviceCpuUsed = helperCpu() - serviceCpuBefore;
        const qint64 rssAfter = residentBytes(), serviceRssAfter = helperRss();
        QVERIFY(heartbeatCount >= 100);
        const double p95 = percentile95(totalHistogram, heartbeatCount);
        const auto finalSamples = controller->property("historySamples").toList();
        qInfo().noquote() << QStringLiteral("Production QML offscreen rendering: final page=%1; readable processes=1000; telemetry=250 ms; history=30 min configured, retained samples=%2; heartbeat=5 ms, p95 upper bound=%3 ms; UI CPU=%4 s (%5% of one core); helper CPU=%6 s (%7% of one core); UI RSS before=%8 after=%9 delta=%10 bytes; helper RSS before=%11 after=%12 delta=%13 bytes; actual elapsed=%14 s; requested=%15 ms. Exercises actual Qt Quick rendering and service IPC; does not qualify compositor/physical Wayland responsiveness.")
            .arg(historySwitched ? QStringLiteral("History") : QStringLiteral("Processes")).arg(finalSamples.size()).arg(p95, 0, 'f', 1)
            .arg(cpuUsed, 0, 'f', 3).arg(cpuUsed / elapsed * 100.0, 0, 'f', 2)
            .arg(serviceCpuUsed, 0, 'f', 3).arg(serviceCpuUsed / elapsed * 100.0, 0, 'f', 2)
            .arg(rssBefore).arg(rssAfter).arg(rssAfter - rssBefore)
            .arg(serviceRssBefore).arg(serviceRssAfter).arg(serviceRssAfter - serviceRssBefore).arg(elapsed, 0, 'f', 3).arg(soakMs);
        if (fullWindowUiRss > 0) qInfo() << "Full History RSS after one-minute render warmup: UI baseline" << fullWindowUiRss << "final" << rssAfter << "delta" << rssAfter - fullWindowUiRss
            << "; helper baseline" << fullWindowServiceRss << "final" << serviceRssAfter << "delta" << serviceRssAfter - fullWindowServiceRss << "bytes.";
        for (const double value : windowP95)
            QVERIFY2(value <= 100.0, qPrintable(QStringLiteral("Production QML five-minute window p95 delay was %1 ms.").arg(value)));
        QVERIFY2(p95 <= 100.0, qPrintable(QStringLiteral("Production QML event-loop p95 delay was %1 ms.").arg(p95)));
        QVERIFY(rssBefore > 0 && rssAfter > 0 && serviceRssBefore > 0 && serviceRssAfter > 0);
        if (soakMs >= 1800000) QVERIFY(historySwitched);
        QVERIFY(!window->grabWindow().isNull());
        QVERIFY2(runtimeErrors.isEmpty(), qPrintable(runtimeErrors.join(QLatin1Char('\n'))));
    }
    void cleanupTestCase() { if (helper.state() != QProcess::NotRunning) { helper.terminate(); helper.waitForFinished(5000); } }
};
QTEST_MAIN(CollectorV09Test)
#include "test_collector_v09.moc"
