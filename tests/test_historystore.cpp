#include "historystore.h"
#include "flightrecorder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace LoofiPulse;

class TestHistoryStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testAggregatesMinMaxAvg()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test-history.sqlite"));

        HistoryStore store;
        QVERIFY(store.open(dbPath, 7ll * 24 * 3600 * 1000));
        QVERIFY(store.isOpen());

        const qint64 baseMs = 1700000040000LL; // Exactly aligned to minute boundary (28333334 * 60000)
        QVariantMap sample1{
            {QStringLiteral("cpuLoadPercent"), 20.0},
            {QStringLiteral("cpuLoadAvailable"), true},
            {QStringLiteral("cpuPackageTemp"), 45.0},
            {QStringLiteral("cpuTempAvailable"), true}
        };
        QVariantMap sample2{
            {QStringLiteral("cpuLoadPercent"), 60.0},
            {QStringLiteral("cpuLoadAvailable"), true},
            {QStringLiteral("cpuPackageTemp"), 65.0},
            {QStringLiteral("cpuTempAvailable"), true}
        };
        QVariantMap sample3{
            {QStringLiteral("cpuLoadPercent"), 40.0},
            {QStringLiteral("cpuLoadAvailable"), true},
            {QStringLiteral("cpuPackageTemp"), 55.0},
            {QStringLiteral("cpuTempAvailable"), true}
        };

        store.addSample(baseMs + 1000, sample1);
        store.addSample(baseMs + 15000, sample2);
        store.addSample(baseMs + 30000, sample3);

        // Advance to next minute to complete bucket
        store.addSample(baseMs + 65000, sample1);
        QVERIFY(store.flush());

        const auto trends = store.trends(baseMs, baseMs + 59000, 100, {QStringLiteral("cpuLoad"), QStringLiteral("cpuTemp")});
        const auto series = trends.value(QStringLiteral("series")).toMap();
        const auto cpuLoadSeries = series.value(QStringLiteral("cpuLoad")).toList();
        QCOMPARE(cpuLoadSeries.size(), 1);

        const auto pt = cpuLoadSeries.first().toMap();
        QCOMPARE(pt.value(QStringLiteral("min")).toDouble(), 20.0);
        QCOMPARE(pt.value(QStringLiteral("max")).toDouble(), 60.0);
        QCOMPARE(pt.value(QStringLiteral("avg")).toDouble(), 40.0);
        QCOMPARE(pt.value(QStringLiteral("n")).toLongLong(), 3);
    }

    void testUnavailableMetricsSkipped()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test-avail.sqlite"));

        HistoryStore store;
        QVERIFY(store.open(dbPath, 86400000LL));

        const qint64 baseMs = 1700000040000LL;
        QVariantMap sample{
            {QStringLiteral("cpuLoadPercent"), 50.0},
            {QStringLiteral("cpuLoadAvailable"), true},
            {QStringLiteral("gpuBusyPercent"), 99.0},
            {QStringLiteral("gpuBusyAvailable"), false} // Unavailable!
        };

        store.addSample(baseMs, sample);
        store.addSample(baseMs + 65000, sample); // flush bucket
        QVERIFY(store.flush());

        const auto trends = store.trends(baseMs, baseMs + 59000, 100, {QStringLiteral("cpuLoad"), QStringLiteral("gpuBusy")});
        const auto series = trends.value(QStringLiteral("series")).toMap();
        QCOMPARE(series.value(QStringLiteral("cpuLoad")).toList().size(), 1);
        QCOMPARE(series.value(QStringLiteral("gpuBusy")).toList().size(), 0);
    }

    void testFlushIsTransactionalAndIdempotent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test-persist.sqlite"));

        const qint64 baseMs = 1700000040000LL;
        {
            HistoryStore store;
            QVERIFY(store.open(dbPath, 86400000LL));
            QVariantMap sample{
                {QStringLiteral("cpuLoadPercent"), 35.0},
                {QStringLiteral("cpuLoadAvailable"), true}
            };
            store.addSample(baseMs, sample);
            store.flush(true); // include partial minute
        }

        // Reopen in fresh instance
        {
            HistoryStore store2;
            QVERIFY(store2.open(dbPath, 86400000LL));
            const auto trends = store2.trends(baseMs - 1000, baseMs + 60000, 100, {QStringLiteral("cpuLoad")});
            const auto series = trends.value(QStringLiteral("series")).toMap();
            const auto list = series.value(QStringLiteral("cpuLoad")).toList();
            QCOMPARE(list.size(), 1);
            QCOMPARE(list.first().toMap().value(QStringLiteral("avg")).toDouble(), 35.0);
        }
    }

    void testReopenResumesPartialMinuteWithoutDuplicateFlushes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("resume.sqlite"));
        const qint64 baseMs = 1700000040000LL;
        HistoryStore store;
        QVERIFY(store.open(dbPath, 86400000LL));
        store.addSample(baseMs + 1000, {{QStringLiteral("cpuLoadAvailable"), true},
                                      {QStringLiteral("cpuLoadPercent"), 20.0}});
        QVERIFY(store.flush(true));
        QVERIFY(store.flush(true));
        store.close();
        QVERIFY(store.open(dbPath, 86400000LL));
        store.addSample(baseMs + 2000, {{QStringLiteral("cpuLoadAvailable"), true},
                                      {QStringLiteral("cpuLoadPercent"), 60.0}});
        for (int i = 0; i < 3; ++i) {
            QVERIFY(store.flush(true));
            const auto points = store.trends(baseMs, baseMs + 59000, 100, {QStringLiteral("cpuLoad")})
                                    .value(QStringLiteral("series")).toMap().value(QStringLiteral("cpuLoad")).toList();
            QCOMPARE(points.size(), 1);
            const auto point = points.first().toMap();
            QCOMPARE(point.value(QStringLiteral("n")).toLongLong(), 2);
            QCOMPARE(point.value(QStringLiteral("avg")).toDouble(), 40.0);
            QCOMPARE(point.value(QStringLiteral("min")).toDouble(), 20.0);
            QCOMPARE(point.value(QStringLiteral("max")).toDouble(), 60.0);
        }
    }

    void testResumeReadErrorDoesNotAddSampleTwice()
    {
        QTemporaryDir dir;
        HistoryStore store;
        QVERIFY(store.open(dir.filePath(QStringLiteral("resume-error.sqlite")), 86400000LL));
        QSqlQuery q(QSqlDatabase::database(store.connectionName()));
        QVERIFY(q.exec(QStringLiteral("ALTER TABLE minute_agg RENAME TO saved_minute_agg;")));
        const QVariantMap sample{{QStringLiteral("cpuLoadAvailable"), true},
                                 {QStringLiteral("cpuLoadPercent"), 20.0}};
        const qint64 baseMs = 1700000040000LL;
        store.addSample(baseMs, sample);
        QVERIFY(!store.lastError().isEmpty());
        QVERIFY(q.exec(QStringLiteral("ALTER TABLE saved_minute_agg RENAME TO minute_agg;")));
        store.addSample(baseMs, sample);
        QVERIFY(store.flush(true));
        const auto points = store.trends(baseMs, baseMs + 59000, 100, {QStringLiteral("cpuLoad")})
                                .value(QStringLiteral("series")).toMap().value(QStringLiteral("cpuLoad")).toList();
        QCOMPARE(points.size(), 1);
        QCOMPARE(points.first().toMap().value(QStringLiteral("n")).toLongLong(), 1);
    }

    void testRetentionPrunes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test-prune.sqlite"));

        HistoryStore store;
        // 1 hour retention
        QVERIFY(store.open(dbPath, 3600000LL));

        const qint64 oldMs = 1700000000000LL;
        const qint64 recentMs = oldMs + 7200000LL; // 2 hours later

        QVariantMap sample{
            {QStringLiteral("cpuLoadPercent"), 25.0},
            {QStringLiteral("cpuLoadAvailable"), true}
        };

        store.addSample(oldMs, sample);
        store.addSample(oldMs + 60000, sample);
        store.flush(true);

        // Add recent sample 2 hours ahead
        store.addSample(recentMs, sample);
        store.addSample(recentMs + 60000, sample);
        store.flush(true);

        // Old minute should have been pruned
        const auto oldTrends = store.trends(oldMs - 1000, oldMs + 60000, 100, {QStringLiteral("cpuLoad")});
        QCOMPARE(oldTrends.value(QStringLiteral("series")).toMap().value(QStringLiteral("cpuLoad")).toList().size(), 0);

        // Recent minute is retained
        const auto recentTrends = store.trends(recentMs - 1000, recentMs + 60000, 100, {QStringLiteral("cpuLoad")});
        QCOMPARE(recentTrends.value(QStringLiteral("series")).toMap().value(QStringLiteral("cpuLoad")).toList().size(), 1);
    }

    void testTrendsDownsamplesWithinMaxPoints()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test-downsample.sqlite"));

        HistoryStore store;
        QVERIFY(store.open(dbPath, 30ll * 86400000LL));

        const qint64 baseMs = 1700000000000LL;
        // Add 300 minutes of data
        for (int m = 0; m < 300; ++m) {
            const qint64 t = baseMs + m * 60000LL;
            QVariantMap sample{
                {QStringLiteral("cpuLoadPercent"), 10.0 + (m % 50)},
                {QStringLiteral("cpuLoadAvailable"), true}
            };
            store.addSample(t, sample);
            store.addSample(t + 30000, sample);
        }
        store.flush(true);

        // Query with maxPoints = 50
        const auto trends = store.trends(baseMs, baseMs + 300 * 60000LL, 50, {QStringLiteral("cpuLoad")});
        const auto list = trends.value(QStringLiteral("series")).toMap().value(QStringLiteral("cpuLoad")).toList();

        // Must downsample to <= 50 points
        QVERIFY(list.size() <= 50);
        QVERIFY(list.size() > 0);
        for (const auto &item : list) {
            const auto pt = item.toMap();
            QVERIFY(pt.value(QStringLiteral("min")).toDouble() <= pt.value(QStringLiteral("avg")).toDouble());
            QVERIFY(pt.value(QStringLiteral("avg")).toDouble() <= pt.value(QStringLiteral("max")).toDouble());
        }
    }

    void testIncidentsUpsertAndRange()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test-incidents.sqlite"));

        HistoryStore store;
        QVERIFY(store.open(dbPath, 86400000LL));

        const qint64 t1 = 1700000000000LL;
        QVariantMap inc1{
            {QStringLiteral("incidentId"), QStringLiteral("inc-cpu-throttle-1")},
            {QStringLiteral("startMs"), t1},
            {QStringLiteral("endMs"), t1 + 30000},
            {QStringLiteral("severity"), 3},
            {QStringLiteral("ruleId"), QStringLiteral("cpu.throttle")},
            {QStringLiteral("headline"), QStringLiteral("CPU Throttled")},
            {QStringLiteral("domain"), QStringLiteral("CPU")}
        };
        store.recordIncident(inc1);

        // Upsert updated endMs
        inc1[QStringLiteral("endMs")] = t1 + 45000;
        store.recordIncident(inc1);

        const auto list = store.incidents(t1 - 1000, t1 + 50000, 10);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().toMap().value(QStringLiteral("incidentId")).toString(), QStringLiteral("inc-cpu-throttle-1"));
        QCOMPARE(list.first().toMap().value(QStringLiteral("endMs")).toLongLong(), t1 + 45000);
        QCOMPARE(list.first().toMap().value(QStringLiteral("severity")).toInt(), 3);
    }

    void testGapsAndCoverage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test-gaps.sqlite"));

        HistoryStore store;
        QVERIFY(store.open(dbPath, 86400000LL));

        const qint64 t1 = 1700000000000LL;
        store.recordGap(t1, t1 + 120000, QStringLiteral("suspend"));

        const auto trends = store.trends(t1 - 1000, t1 + 200000, 100, {});
        const auto gaps = trends.value(QStringLiteral("gaps")).toList();
        QCOMPARE(gaps.size(), 1);
        QCOMPARE(gaps.first().toMap().value(QStringLiteral("reason")).toString(), QStringLiteral("suspend"));
        QCOMPARE(gaps.first().toMap().value(QStringLiteral("startMs")).toLongLong(), t1);
    }

    void testCorruptFileRecovered()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("corrupt.sqlite"));

        // Write corrupt bytes
        {
            QFile f(dbPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("GARBAGE DATA THAT IS NOT SQLITE HEADER");
            f.close();
        }

        HistoryStore store;
        // Opening should detect corruption, rename corrupt file and open fresh DB
        QVERIFY(store.open(dbPath, 86400000LL));
        QVERIFY(store.isOpen());
        QVERIFY(!store.recoveryNotice().isEmpty());

        // A backup file must exist
        const auto entries = QDir(dir.path()).entryList(QStringList{QStringLiteral("corrupt.sqlite.corrupt-*.sqlite")});
        QCOMPARE(entries.size(), 1);
    }

    void testUnwritableLocationFails()
    {
        HistoryStore store;
        QVERIFY(!store.open(QStringLiteral("/proc/sys/kernel/cannot_write_here.sqlite"), 86400000LL));
        QVERIFY(!store.isOpen());
        QVERIFY(!store.lastError().isEmpty());
    }

    void testNoProcessIdentityColumns()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test-privacy.sqlite"));

        HistoryStore store;
        QVERIFY(store.open(dbPath, 86400000LL));

        const QStringList tables = {QStringLiteral("minute_agg"), QStringLiteral("incidents"), QStringLiteral("gaps")};
        for (const auto &tbl : tables) {
            // Check pragma table_info
            QSqlQuery q(QStringLiteral("PRAGMA table_info(%1);").arg(tbl), QSqlDatabase::database(store.connectionName()));
            QVERIFY(q.exec());
            while (q.next()) {
                const QString colName = q.value(1).toString().toLower();
                QVERIFY(!colName.contains(QStringLiteral("pid")));
                QVERIFY(!colName.contains(QStringLiteral("process")));
                QVERIFY(!colName.contains(QStringLiteral("cmd")));
            }
        }

        // Check columns of all tables in schema
        for (const auto &spec : HistoryStore::metrics()) {
            QVERIFY(!spec.id.contains(QStringLiteral("pid"), Qt::CaseInsensitive));
            QVERIFY(!spec.id.contains(QStringLiteral("process"), Qt::CaseInsensitive));
            QVERIFY(!spec.id.contains(QStringLiteral("cmd"), Qt::CaseInsensitive));
        }
    }

    void testSizeCapPreservesRecentDataAndReclaimsFiles()
    {
        QTemporaryDir dir;
        HistoryStore store;
        QVERIFY(store.open(dir.filePath(QStringLiteral("bounded.sqlite")), 31LL * 86400000LL, 512 * 1024));
        const qint64 base = 1700000040000LL;
        QVariantMap live;
        for (const auto &metric : HistoryStore::metrics()) {
            live.insert(metric.valueKey, 42.0);
            if (!metric.availableKey.isEmpty()) live.insert(metric.availableKey, true);
        }
        for (int minute = 0; minute < 1800; ++minute) {
            store.addSample(base + minute * 60000LL, live);
            if (minute % 60 == 59) QVERIFY2(store.flush(true), qPrintable(store.lastError()));
        }
        QVERIFY2(store.flush(true), qPrintable(store.lastError()));
        QVERIFY(store.oldestMs() > base);
        QCOMPARE(store.newestMs(), base + 1799 * 60000LL);
        QVERIFY2(store.sizeBytes() <= store.maxBytes(), qPrintable(QString::number(store.sizeBytes())));
        const auto data = store.trends(base, base + 1800 * 60000LL, 2000, {QStringLiteral("cpuLoad")});
        QVERIFY(!data.value(QStringLiteral("series")).toMap().value(QStringLiteral("cpuLoad")).toList().isEmpty());
        QVERIFY(store.flush(true));
        QCOMPARE(store.newestMs(), base + 1799 * 60000LL);
    }

    void testLegacyMigrationMakesConsistentBackupAndKeepsUnknownTimes()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("legacy.sqlite"));
        const QString connection = QStringLiteral("legacy-fixture");
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(path); QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA journal_mode=WAL;")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE incidents(id TEXT PRIMARY KEY, start_ms INTEGER NOT NULL, end_ms INTEGER NOT NULL, severity INTEGER NOT NULL, rule_id TEXT, headline TEXT, domain TEXT);")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO incidents VALUES('legacy', 1000, 2000, 2, 'cpu', 'Legacy incident', 'CPU');")));
        }
        // Keep the fixture connection open so committed incident data may still live in its WAL.
        HistoryStore store;
        QVERIFY2(store.open(path, 86400000LL), qPrintable(store.lastError()));
        QSqlDatabase::removeDatabase(connection);
        QCOMPARE(store.legacyIncidentCount(), 1);
        QVERIFY(store.incidents(0, 3000, 10).isEmpty());
        QVERIFY(!store.recoveryNotice().isEmpty());
        QCOMPARE(QDir(dir.path()).entryList({QStringLiteral("*.pre-v1-backup-*.sqlite")}).size(), 1);
        const QString backup = dir.filePath(QDir(dir.path()).entryList({QStringLiteral("*.pre-v1-backup-*.sqlite")}).first());
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(backup); QVERIFY(db.open());
            QSqlQuery q(db); QVERIFY(q.exec(QStringLiteral("SELECT id FROM incidents;"))); QVERIFY(q.next());
            QCOMPARE(q.value(0).toString(), QStringLiteral("legacy"));
            QVERIFY(q.exec(QStringLiteral("PRAGMA user_version;"))); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 0);
        }
        QSqlDatabase::removeDatabase(connection);
        store.addSample(1700000040000LL, {{QStringLiteral("cpuLoadAvailable"), true}, {QStringLiteral("cpuLoadPercent"), 10.0}});
        QVERIFY(store.flush(true));
        QCOMPARE(store.legacyIncidentCount(), 1);
        store.close(); QVERIFY(store.open(path, 86400000LL));
        QCOMPARE(store.legacyIncidentCount(), 1);
        QCOMPARE(QDir(dir.path()).entryList({QStringLiteral("*.pre-v1-backup-*.sqlite")}).size(), 1);
    }

    void testRecorderIncidentsUseSampleCalendarTimes()
    {
        QTemporaryDir dir;
        HistoryStore store; QVERIFY(store.open(dir.filePath(QStringLiteral("epochs.sqlite")), 86400000LL));
        FlightRecorder recorder;
        QMap<qulonglong, qint64> times;
        for (int i = 1; i <= 2; ++i) {
            FlightSample sample; sample.sampleId = i; sample.monotonicMs = i * 1000;
            sample.timestampMs = 1700000040000LL + i * 1000;
            sample.severity = VerdictSeverity::Bottleneck;
            recorder.appendSample(sample); times.insert(i, sample.timestampMs);
        }
        const auto incident = recorder.incidentEpisodes().first().toMap();
        QVERIFY(store.recordIncident(incident, times));
        const auto found = store.incidents(1700000040000LL, 1700000100000LL, 10);
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().toMap().value(QStringLiteral("startMs")).toLongLong(), times.value(1));
        QCOMPARE(found.first().toMap().value(QStringLiteral("endMs")).toLongLong(), times.value(2));
        QVERIFY(store.recordIncident(incident, {})); // A trimmed lookup must not downgrade known calendar time.
        QCOMPARE(store.incidents(1700000040000LL, 1700000100000LL, 10).size(), 1);
        auto extended = incident;
        extended[QStringLiteral("endSampleId")] = 3;
        QVERIFY(store.recordIncident(extended, {{3, 1700000043000LL}}));
        QCOMPARE(store.incidents(1700000040000LL, 1700000100000LL, 10).first().toMap().value(QStringLiteral("endMs")).toLongLong(), 1700000043000LL);
        QCOMPARE(incident.value(QStringLiteral("startMs")).toLongLong(), 1000); // Session semantics stay monotonic.
    }

    void testWriteErrorsAreVisibleInTrends()
    {
        QTemporaryDir dir;
        HistoryStore store; QVERIFY(store.open(dir.filePath(QStringLiteral("errors.sqlite")), 86400000LL));
        QSqlQuery q(QSqlDatabase::database(store.connectionName()));
        QVERIFY(q.exec(QStringLiteral("PRAGMA query_only=ON;")));
        store.addSample(1700000040000LL, {{QStringLiteral("cpuLoadAvailable"), true}, {QStringLiteral("cpuLoadPercent"), 10.0}});
        QVERIFY(!store.flush(true));
        QVERIFY(!store.lastError().isEmpty());
        QVERIFY(!store.trends(1700000000000LL, 1700000100000LL, 100, {}).value(QStringLiteral("storageError")).toString().isEmpty());
        QVERIFY(!store.clear());
        QVERIFY(q.exec(QStringLiteral("PRAGMA query_only=OFF;")));
        QVERIFY(store.flush(true));
        QVERIFY(store.lastError().isEmpty());
        QVERIFY(store.trends(1700000000000LL, 1700000100000LL, 100, {}).value(QStringLiteral("storageError")).toString().isEmpty());
    }

    void testReadErrorsAndRecoveryAreExplicit()
    {
        QTemporaryDir dir;
        HistoryStore store; QVERIFY(store.open(dir.filePath(QStringLiteral("read-errors.sqlite")), 86400000LL));
        QSqlQuery q(QSqlDatabase::database(store.connectionName()));
        QVERIFY(q.exec(QStringLiteral("ALTER TABLE incidents RENAME TO saved_incidents;")));
        QString error;
        QVERIFY(store.incidents(0, 1700000100000LL, 100, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(q.exec(QStringLiteral("ALTER TABLE saved_incidents RENAME TO incidents;")));
        store.incidents(0, 1700000100000LL, 100, &error);
        QVERIFY(error.isEmpty());
        for (const QString &table : {QStringLiteral("minute_agg"), QStringLiteral("gaps")}) {
            QVERIFY(q.exec(QStringLiteral("ALTER TABLE %1 RENAME TO saved_table;").arg(table)));
            const auto failed = store.trends(1, 1700000100000LL, 100, {}, &error);
            QVERIFY(!error.isEmpty());
            QVERIFY(!failed.value(QStringLiteral("storageError")).toString().isEmpty());
            QVERIFY(!failed.contains(QStringLiteral("series"))); // Never present partial data as complete.
            QVERIFY(q.exec(QStringLiteral("ALTER TABLE saved_table RENAME TO %1;").arg(table)));
            const auto recovered = store.trends(1, 1700000100000LL, 100, {}, &error);
            QVERIFY(error.isEmpty());
            QVERIFY(recovered.value(QStringLiteral("storageError")).toString().isEmpty());
        }
    }

    void testClear()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test-clear.sqlite"));

        HistoryStore store;
        QVERIFY(store.open(dbPath, 86400000LL));

        const qint64 baseMs = 1700000000000LL;
        QVariantMap sample{
            {QStringLiteral("cpuLoadPercent"), 50.0},
            {QStringLiteral("cpuLoadAvailable"), true}
        };
        store.addSample(baseMs, sample);
        store.flush(true);

        QVERIFY(store.oldestMs() > 0);
        QVERIFY(store.clear());
        QCOMPARE(store.oldestMs(), 0);
        QCOMPARE(store.newestMs(), 0);
    }
};

QTEST_MAIN(TestHistoryStore)
#include "test_historystore.moc"
