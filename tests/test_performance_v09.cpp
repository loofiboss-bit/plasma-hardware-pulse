#include "cpusocsampler.h"
#include "flightrecorder.h"
#include <QtTest>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <atomic>
#include <time.h>
#include <unistd.h>

using namespace LoofiPulse;
namespace {
bool writeFixture(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(content) == content.size();
}
qint64 residentBytes()
{
    QFile file(QStringLiteral("/proc/self/statm"));
    if (!file.open(QIODevice::ReadOnly)) return -1;
    const auto fields = file.readAll().simplified().split(' ');
    return fields.size() > 1 ? fields.at(1).toLongLong() * sysconf(_SC_PAGESIZE) : -1;
}
double cpuSeconds()
{
    timespec time{};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time);
    return double(time.tv_sec) + double(time.tv_nsec) / 1000000000.0;
}
QVariantList observations(const ProcessRadarSnapshot &radar, qint64 observedAt)
{
    QVariantList rows;
    for (const auto &p : radar.historicalProcesses) {
        rows.append(QVariantMap{{QStringLiteral("pid"), p.pid}, {QStringLiteral("name"), p.name},
            {QStringLiteral("startTimeTicks"), QVariant::fromValue(p.startTimeTicks)},
            {QStringLiteral("cpuPercent"), p.cpuPercent}, {QStringLiteral("rssMegabytes"), p.rssMegabytes},
            {QStringLiteral("inDiskWait"), p.inDiskWait}, {QStringLiteral("observedAtMs"), observedAt}});
    }
    return rows;
}
}
class PerformanceV09Test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void productionCoreRetentionAndWorkerResponsiveness()
    {
        QTemporaryDir fixture;
        QVERIFY(fixture.isValid());
        const QString proc = fixture.filePath(QStringLiteral("proc"));
        const QString sys = fixture.filePath(QStringLiteral("sys"));
        QVERIFY(writeFixture(proc + QStringLiteral("/stat"), "cpu 100000 0 0 100000 0 0 0 0\ncpu0 100000 0 0 100000 0 0 0 0\n"));
        for (int pid = 1; pid <= 1000; ++pid) {
            QStringList fields(22, QStringLiteral("0"));
            fields[0] = pid % 7 == 0 ? QStringLiteral("D") : QStringLiteral("R");
            fields[11] = QString::number(pid * 10); fields[19] = QString::number(pid * 100);
            fields[21] = QString::number(1024 + pid);
            const auto line = QStringLiteral("%1 (fixture-worker-%1) %2\n").arg(pid).arg(fields.join(QLatin1Char(' ')));
            QVERIFY(writeFixture(proc + QStringLiteral("/%1/stat").arg(pid), line.toUtf8()));
        }
        struct Results {
            QString error;
            int scans = 0;
            int processCount = 0;
            int historicalCount = 0;
            int firstCount = 0, secondCount = 0;
            int firstCapacity = 0, secondCapacity = 0, thirdCapacity = 0;
            qint64 rssFirst = -1, rssSecond = -1, rssSteady = -1;
            QList<double> scanMs;
        } results;
        std::atomic<int> inFlight{0}, maximumInFlight{0};
        const double cpuStart = cpuSeconds();
        QElapsedTimer total; total.start();
        auto *worker = QThread::create([&] {
            CpuSocSampler sampler({sys, proc});
            sampler.sampleCpu({});
            const auto radar = sampler.sampleProcessRadar(8);
            results.processCount = int(radar.allProcesses.size());
            results.historicalCount = int(radar.historicalProcesses.size());
            FlightRecorder recorder;
            recorder.setRetentionMs(30 * 60 * 1000);
            QVariantList retainedProcesses;
            // Virtual timestamps exercise 90 minutes of the production recorder at 250 ms.
            // Process snapshots follow the production two-second cadence, with distinct timestamps.
            for (int i = 0; i <= 21600; ++i) {
                if (i % 8 == 0) retainedProcesses = observations(radar, 1000 + qint64(i) * 250);
                FlightSample sample;
                sample.monotonicMs = 1000 + qint64(i) * 250;
                sample.timestampMs = 1700000000000LL + qint64(i) * 250;
                sample.elapsedSincePreviousMs = i ? 250 : 0;
                sample.cpuLoadPercent = double(i % 101);
                sample.metricQuality = {{QStringLiteral("cpuLoadAvailable"), true}, {QStringLiteral("psiAvailable"), true}};
                sample.processObservations = retainedProcesses;
                sample.severity = i % 200 < 100 ? VerdictSeverity::Elevated : VerdictSeverity::Nominal;
                recorder.appendSample(std::move(sample));
                if (i == 7200) { results.firstCount = recorder.count(); results.rssFirst = residentBytes(); results.firstCapacity = int(recorder.samples().capacity()); }
                if (i == 14400) { results.secondCount = recorder.count(); results.rssSecond = residentBytes(); results.secondCapacity = int(recorder.samples().capacity()); }
            }
            results.rssSteady = residentBytes(); results.thirdCapacity = int(recorder.samples().capacity());
            // Real-time filesystem scans exercise the production /proc reader under a GUI event loop.
            // This deliberately scans four times/second, exceeding the normal process cadence.
            QElapsedTimer realTime; realTime.start();
            while (realTime.elapsed() < 10000 && !QThread::currentThread()->isInterruptionRequested()) {
                const int active = inFlight.fetch_add(1) + 1;
                maximumInFlight.store(std::max(maximumInFlight.load(), active));
                QElapsedTimer scan; scan.start();
                sampler.sampleCpu({});
                const auto next = sampler.sampleProcessRadar(8);
                results.scanMs.append(double(scan.nsecsElapsed()) / 1000000.0);
                if (next.allProcesses.size() != 1000) results.error = QStringLiteral("The production scanner did not retain all 1,000 processes.");
                ++results.scans;
                inFlight.fetch_sub(1);
                const qint64 remaining = 250 - scan.elapsed();
                if (remaining > 0) QThread::msleep(static_cast<unsigned long>(remaining));
            }
        });
        QEventLoop loop;
        QTimer heartbeat;
        heartbeat.setTimerType(Qt::PreciseTimer);
        heartbeat.setInterval(5);
        QElapsedTimer heartClock; heartClock.start();
        qint64 previousBeat = 0;
        QList<double> delays;
        connect(&heartbeat, &QTimer::timeout, &loop, [&] {
            const qint64 now = heartClock.nsecsElapsed();
            if (previousBeat) delays.append(std::max(0.0, double(now - previousBeat) / 1000000.0 - 5.0));
            previousBeat = now;
        });
        connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
        QTimer deadline;
        deadline.setSingleShot(true);
        bool timedOut = false;
        connect(&deadline, &QTimer::timeout, &loop, [&] { timedOut = true; worker->requestInterruption(); loop.quit(); });
        heartbeat.start(); deadline.start(30000); worker->start(); loop.exec();
        worker->wait(); delete worker;
        heartbeat.stop();
        QVERIFY2(!timedOut, "The production core performance workload timed out.");
        QVERIFY2(results.error.isEmpty(), qPrintable(results.error));
        QCOMPARE(results.processCount, 1000);
        QVERIFY(results.historicalCount > 0 && results.historicalCount <= 24);
        QCOMPARE(results.firstCount, 7201); QCOMPARE(results.secondCount, 7201);
        QCOMPARE(maximumInFlight.load(), 1);
        QVERIFY(results.scans >= 10);
        QVERIFY(results.rssFirst > 0 && results.rssSecond > 0);
        qInfo() << "RSS retention windows" << results.rssFirst << results.rssSecond << results.rssSteady << "capacities" << results.firstCapacity << results.secondCapacity << results.thirdCapacity;
        // The allocator can retain freed pages; allow bounded 8 MiB drift after capacity fills.
        QVERIFY2(results.rssSecond - results.rssFirst <= 8 * 1024 * 1024, "Resident memory continued growing after the retention window filled.");
        QVERIFY2(results.rssSteady - results.rssSecond <= 2 * 1024 * 1024, "Resident memory did not reach a steady state during a third retention window.");
        QVERIFY(delays.size() >= 100);
        std::sort(delays.begin(), delays.end());
        const double p95 = delays.at((delays.size() - 1) * 95 / 100);
        QVERIFY2(p95 <= 100.0, qPrintable(QStringLiteral("Event-loop p95 delay was %1 ms.").arg(p95)));
        double sum = 0, maximumScan = 0;
        for (double value : results.scanMs) { sum += value; maximumScan = std::max(maximumScan, value); }
        const double wallSeconds = double(total.nsecsElapsed()) / 1000000000.0;
        qInfo().noquote() << QStringLiteral("core workload: virtual history=90 min, interval=250 ms, samples=21601, retained=%1/%2; processes=1000; scans=%3; scan mean=%4 ms max=%5 ms; heartbeat p95=%6 ms; RSS first=%7 second=%8 delta=%9 bytes; CPU=%10 s (%11% of one core), wall=%12 s; maximum in-flight=%13; steady-state RSS=%14 delta=%15 bytes")
            .arg(results.firstCount).arg(results.secondCount).arg(results.scans)
            .arg(sum / results.scans, 0, 'f', 3).arg(maximumScan, 0, 'f', 3).arg(p95, 0, 'f', 3)
            .arg(results.rssFirst).arg(results.rssSecond).arg(results.rssSecond - results.rssFirst)
            .arg(cpuSeconds() - cpuStart, 0, 'f', 3).arg((cpuSeconds() - cpuStart) / wallSeconds * 100.0, 0, 'f', 2)
            .arg(wallSeconds, 0, 'f', 3).arg(maximumInFlight.load()).arg(results.rssSteady).arg(results.rssSteady - results.rssSecond);
    }
};
QTEST_GUILESS_MAIN(PerformanceV09Test)
#include "test_performance_v09.moc"
