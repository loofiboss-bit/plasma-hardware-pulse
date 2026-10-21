#include <QtTest>
#include <limits>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "flightrecorder.h"
#include "sessionstore.h"
using namespace LoofiPulse;
class SessionsV09Test : public QObject {
    Q_OBJECT
private:
    static FlightSample sample(quint64 id, qint64 time, VerdictSeverity severity = VerdictSeverity::Nominal) {
        FlightSample s;
        s.sampleId = id; s.timestampMs = 100000 + time; s.monotonicMs = time;
        s.severity = severity; s.verdictHeadline = QStringLiteral("Original diagnosis");
        s.metricQuality = {{QStringLiteral("cpuLoadAvailable"), true}, {QStringLiteral("psiAvailable"), true}};
        s.cpuLoadPercent = double(id); return s;
    }
private Q_SLOTS:
    void newRecordingUsesCurrentEngineVersion() {
        FlightRecorder recorder;
        recorder.appendSample(sample(1, 1000));
        QCOMPARE(recorder.freezeRange(1, 1).engineVersion, QStringLiteral(LOOFI_PULSE_VERSION));
        auto imported = recorder.freezeRange(1, 1);
        imported.engineVersion = QStringLiteral("original-historical-engine");
        recorder.restoreSnapshot(imported);
        QCOMPARE(recorder.freezeRange(1, 1).engineVersion, imported.engineVersion);
        recorder.clear();
        recorder.appendSample(sample(2, 2000));
        QCOMPARE(recorder.freezeRange(2, 2).engineVersion, QStringLiteral(LOOFI_PULSE_VERSION));
    }
    void codecRoundtrip() {
        auto s = sample(1, 1000);
        s.ramTotalBytes = std::numeric_limits<quint64>::max();
        s.processObservations = {QVariantMap{{QStringLiteral("name"), QStringLiteral("private-command")}, {QStringLiteral("pid"), 42}}};
        s.gpuDevices = {QVariantMap{{QStringLiteral("id"), QStringLiteral("pci:01")}, {QStringLiteral("available"), true}}};
        s.verdictEvidence = {QStringLiteral("Original evidence")};
        s.verdictRuleId = QStringLiteral("rule-original");
        SessionSnapshot input;
        input.recordingId = QStringLiteral("original-recording"); input.engineVersion = QStringLiteral("original-engine"); input.samples = {s};
        QString error;
        const auto bytes = SessionStore::encode(input, true, &error);
        QVERIFY2(!bytes.isEmpty(), qPrintable(error));
        SessionSnapshot output;
        QVERIFY2(SessionStore::decode(bytes, &output, &error), qPrintable(error));
        QCOMPARE(output.engineVersion, input.engineVersion);
        QCOMPARE(SessionStore::sampleToMap(output.samples.first()), SessionStore::sampleToMap(s));
    }
    void privacyAndAtomicWrite() {
        QTemporaryDir dir;
        SessionSnapshot input;
        auto s = sample(1, 1000);
        s.culpritName = QStringLiteral("private-command"); s.culpritPid = 42;
        s.metricQuality.insert(QStringLiteral("nested"), QVariantMap{{QStringLiteral("processName"), QStringLiteral("private-nested")}});
        s.metricQuality.insert(QStringLiteral("metricMetadata"), QVariantMap{{QStringLiteral("cpuLoad"), QVariantMap{
            {QStringLiteral("state"), QStringLiteral("available")}, {QStringLiteral("source"), QStringLiteral("private-command PID 42")},
            {QStringLiteral("detail"), QStringLiteral("private-command PID 42")}}}});
        s.processObservations = {QVariantMap{{QStringLiteral("name"), QStringLiteral("private-list")}, {QStringLiteral("pid"), 42}}};
        s.verdictEvidence = {QStringLiteral("private-command PID 42"), QStringLiteral("CPU PSI exceeded the sustained threshold")};
        s.verdictCausalChain = QStringLiteral("Thermal pressure coincided with private-list");
        s.verdictHeadline = QStringLiteral("private-command observed with high pressure");
        s.contributingSignals = {QStringLiteral("CPU pressure"), QStringLiteral("private-list activity")};
        input.metadata.insert(QStringLiteral("description"), QStringLiteral("private-command and private-list"));
        input.samples = {s};
        const auto bytes = SessionStore::encode(input);
        QVERIFY(!bytes.isEmpty()); QVERIFY(!bytes.contains("private-")); QVERIFY(!bytes.contains("culpritPid"));
        const QString path = dir.filePath(QStringLiteral("test.pulse-session"));
        QString error;
        QVERIFY2(SessionStore::write(path, input, false, &error), qPrintable(error));
        SessionSnapshot out;
        QVERIFY(SessionStore::read(path, &out, &error));
        QCOMPARE(out.samples.first().culpritPid, 0);
        QVERIFY(out.samples.first().verdictEvidence.contains(QStringLiteral("CPU PSI exceeded the sustained threshold")));
        QVERIFY(out.samples.first().contributingSignals.contains(QStringLiteral("CPU pressure")));
        QVERIFY(out.samples.first().verdictCausalChain.startsWith(QStringLiteral("Thermal pressure")));
        QVERIFY(out.samples.first().verdictHeadline.contains(QStringLiteral("high pressure")));
        FlightRecorder recorder;
        s.sampleId = 0;
        recorder.appendSample(s);
        const auto report = recorder.exportSamplesJson();
        QVERIFY(!report.contains(QStringLiteral("private-")));
        QVERIFY(!report.contains(QStringLiteral("PID 42")));
        QVERIFY(report.contains(QStringLiteral("CPU PSI exceeded the sustained threshold")));
        QVERIFY(report.contains(QStringLiteral("verdictCausalChain")));
        QVERIFY(report.contains(QStringLiteral("gpuDevices")));
        QVERIFY(!report.contains(QStringLiteral("processObservations")));
        QVERIFY(recorder.exportSummaryMarkdown().contains(QStringLiteral("Report scope")));
        QVERIFY(!recorder.exportSummaryMarkdown().contains(QStringLiteral("private-")));
        QVERIFY(!recorder.exportSummaryMarkdown().contains(QStringLiteral("PID 42")));
        QVERIFY(!SessionStore::write(dir.path(), input, false, &error));
        QVERIFY(error.contains(QStringLiteral("Could not open the session file")));
        QVERIFY(SessionStore::read(path, &out, &error));
    }
    void rejectedLoadIsTransactional() {
        SessionSnapshot original;
        original.recordingId = QStringLiteral("keep-me"); original.samples = {sample(1, 1000)};
        QString error;
        QVERIFY(!SessionStore::decode("{", &original, &error));
        QCOMPARE(original.recordingId, QStringLiteral("keep-me"));
        auto object = QJsonDocument::fromJson(SessionStore::encode(original)).object();
        auto samples = object.value(QStringLiteral("samples")).toArray(); samples.append(samples.first());
        object.insert(QStringLiteral("samples"), samples);
        QVERIFY(!SessionStore::decode(QJsonDocument(object).toJson(), &original, &error));
        QCOMPARE(original.samples.size(), 1);
        object.insert(QStringLiteral("version"), 2);
        QVERIFY(!SessionStore::decode(QJsonDocument(object).toJson(), &original, &error));
        QVERIFY(!SessionStore::decode(QByteArray(SessionStore::MaximumBytes + 1, ' '), &original, &error));
        object.insert(QStringLiteral("version"), 1);
        object.insert(QStringLiteral("samples"), QJsonArray{});
        QVERIFY(!SessionStore::decode(QJsonDocument(object).toJson(), &original, &error));
        QCOMPARE(original.recordingId, QStringLiteral("keep-me"));
    }
    void reportRedactionUsesAllOriginalSamples() {
        FlightRecorder recorder;
        auto first = sample(1, 1000);
        first.culpritName = QStringLiteral("private-command"); first.culpritPid = 42;
        recorder.appendSample(first);
        auto later = sample(2, 2000);
        later.verdictHeadline = QStringLiteral("private-command PID 42 remained under pressure");
        later.verdictEvidence = {later.verdictHeadline};
        later.metricQuality[QStringLiteral("metricMetadata")] = QVariantMap{{QStringLiteral("cpuLoad"), QVariantMap{
            {QStringLiteral("state"), QStringLiteral("available")}, {QStringLiteral("source"), later.verdictHeadline},
            {QStringLiteral("detail"), later.verdictHeadline}}}};
        recorder.appendSample(later);
        const auto json = recorder.exportSamplesJson();
        const auto markdown = recorder.exportSummaryMarkdown();
        QVERIFY(!json.contains(QStringLiteral("private-command"))); QVERIFY(!json.contains(QStringLiteral("PID 42")));
        QVERIFY(!markdown.contains(QStringLiteral("private-command"))); QVERIFY(!markdown.contains(QStringLiteral("PID 42")));
        const auto row = QJsonDocument::fromJson(json.toUtf8()).array().last().toObject();
        QVERIFY(row.contains(QStringLiteral("culpritPid"))); QCOMPARE(row.value(QStringLiteral("culpritPid")).toInt(), 0);
        QVERIFY(recorder.exportSamplesJson(true).contains(QStringLiteral("private-command")));
    }
    void invalidTypesReferencesAndLimits() {
        SessionSnapshot input;
        input.samples = {sample(1, 1000)};
        auto root = QJsonDocument::fromJson(SessionStore::encode(input)).object();
        auto sampleObject = root.value(QStringLiteral("samples")).toArray().first().toObject();
        sampleObject.insert(QStringLiteral("cpuLoadPercent"), QStringLiteral("not-a-number"));
        root.insert(QStringLiteral("samples"), QJsonArray{sampleObject});
        SessionSnapshot output;
        QString error;
        QVERIFY(!SessionStore::decode(QJsonDocument(root).toJson(), &output, &error));
        root = QJsonDocument::fromJson(SessionStore::encode(input)).object();
        sampleObject = root.value(QStringLiteral("samples")).toArray().first().toObject();
        sampleObject.remove(QStringLiteral("metricQuality"));
        root.insert(QStringLiteral("samples"), QJsonArray{sampleObject});
        QVERIFY(!SessionStore::decode(QJsonDocument(root).toJson(), &output, &error));
        input.incidents = {QVariantMap{{QStringLiteral("incidentId"), QStringLiteral("episode")}, {QStringLiteral("sampleId"), 999}}};
        QVERIFY(SessionStore::encode(input, false, &error).isEmpty());
        input.incidents.clear();
        input.metadata = {{QStringLiteral("selectionStartSampleId"), 999}};
        QVERIFY(SessionStore::encode(input, false, &error).isEmpty());
        input.metadata.clear();
        input.samples = QList<FlightSample>(SessionStore::MaximumSamples + 1, sample(1, 1000));
        QVERIFY(SessionStore::encode(input, false, &error).isEmpty());
    }
    void incidentIdentityAndFrozenSelection() {
        FlightRecorder recorder(10);
        recorder.appendSample(sample(1, 1000));
        recorder.appendSample(sample(2, 2000, VerdictSeverity::Elevated));
        const QString id = recorder.incidentEpisodes().first().toMap().value(QStringLiteral("incidentId")).toString();
        recorder.appendSample(sample(3, 3000, VerdictSeverity::CriticalThrottle));
        QCOMPARE(recorder.incidentEpisodes().first().toMap().value(QStringLiteral("incidentId")).toString(), id);
        QCOMPARE(recorder.incidentEpisodes().first().toMap().value(QStringLiteral("sampleId")).toULongLong(), quint64(3));
        QVERIFY(!recorder.markProblemNow().isEmpty());
        recorder.appendSample(sample(4, 4000));
        QCOMPARE(recorder.incidentEpisodes().first().toMap().value(QStringLiteral("recoverySampleId")).toULongLong(), quint64(4));
        const auto frozen = recorder.freezeIncident(id);
        QCOMPARE(frozen.samples.size(), 4);
        for (quint64 i = 5; i <= 25; ++i) recorder.appendSample(sample(i, qint64(i * 1000)));
        QCOMPARE(frozen.samples.size(), 4);
        QCOMPARE(frozen.samples.at(1).sampleId, quint64(2));
        auto imported = frozen;
        imported.engineVersion = QStringLiteral("original-historical-engine");
        FlightRecorder restored;
        restored.restoreSnapshot(imported);
        const auto selected = restored.freezeIncident(id);
        QCOMPARE(selected.metadata.value(QStringLiteral("incidentId")).toString(), id);
        QCOMPARE(selected.recordingId, frozen.recordingId);
        QCOMPARE(selected.engineVersion, imported.engineVersion);
        QCOMPARE(selected.samples.size(), frozen.samples.size());
        QCOMPARE(restored.markers(), frozen.markers);
        QString error;
        QVERIFY2(!SessionStore::encode(frozen, false, &error).isEmpty(), qPrintable(error));
    }
    void gapSplitsAndComparisonCoverage() {
        FlightRecorder recorder;
        recorder.appendSample(sample(1, 1000, VerdictSeverity::Elevated));
        auto gap = sample(2, 10000, VerdictSeverity::Elevated);
        gap.metricQuality.insert(QStringLiteral("gapBefore"), true);
        recorder.appendSample(gap);
        const auto episodes = recorder.incidentEpisodes();
        QCOMPARE(episodes.size(), 2);
        QVERIFY(episodes.first().toMap().value(QStringLiteral("gapTerminated")).toBool());
        const auto comparison = recorder.compareRange(2, 2);
        QVERIFY(!comparison.value(QStringLiteral("after")).toMap().value(QStringLiteral("available")).toBool());
        QVERIFY(!comparison.value(QStringLiteral("before")).toMap().value(QStringLiteral("complete")).toBool());
    }
    void restoredPartialIncidentUsesRetainedEndpoints() {
        FlightRecorder recorder;
        for (quint64 i = 1; i <= 100; ++i) recorder.appendSample(sample(i, qint64(i * 1000), VerdictSeverity::Elevated));
        const QString id = recorder.incidentEpisodes().first().toMap().value(QStringLiteral("incidentId")).toString();
        const auto range = recorder.freezeRange(5, 6);
        QVERIFY(!range.incidents.first().toMap().contains(QStringLiteral("endSampleId")));
        FlightRecorder restored;
        restored.restoreSnapshot(range);
        const auto selected = restored.freezeIncident(id);
        QCOMPARE(selected.samples.size(), range.samples.size());
        QCOMPARE(selected.metadata.value(QStringLiteral("incidentId")).toString(), id);
        QString error;
        QVERIFY2(!SessionStore::encode(selected, false, &error).isEmpty(), qPrintable(error));
    }
};
QTEST_GUILESS_MAIN(SessionsV09Test)
#include "test_sessions_v09.moc"
