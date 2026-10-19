#include "historystore.h"

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
