#include "cpusocsampler.h"
#include "powerflowsampler.h"
#include "psimemorysampler.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace LoofiPulse;
namespace {
void write(const QString &path, const QString &text) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(text.toUtf8()), text.toUtf8().size());
}
QString process(int pid, int rss, const QString &state = QStringLiteral("S")) {
    QStringList fields; for (int i = 0; i < 22; ++i) fields.append(QStringLiteral("0"));
    fields[0] = state; fields[19] = QString::number(pid); fields[21] = QString::number(rss);
    return QStringLiteral("%1 (worker) ").arg(pid) + fields.join(QLatin1Char(' '));
}
}
class TestHardwareV09 : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void processListIncludesSmallIdleAndInit() {
        QTemporaryDir temp;
        const auto proc = temp.filePath(QStringLiteral("proc"));
        for (int pid = 1; pid <= 40; ++pid) write(proc + QStringLiteral("/%1/stat").arg(pid), process(pid, pid, pid > 30 ? QStringLiteral("D") : QStringLiteral("S")));
        CpuSocSampler sampler({temp.filePath(QStringLiteral("sys")), proc});
        const auto radar = sampler.sampleProcessRadar();
        QCOMPARE(radar.allProcesses.size(), 40); QCOMPARE(radar.diskWaitProcessCount, 10);
        QVERIFY(radar.historicalProcesses.size() <= 24); QVERIFY(radar.historicalProcesses.size() >= 8);
        for (const auto &row : radar.allProcesses) { QVERIFY(row.ownerAvailable); QVERIFY(!row.cpuAvailable); QCOMPARE(row.startTimeTicks, quint64(row.pid)); }
    }
    void multipleGpuAndUnavailableDevice() {
        QTemporaryDir temp; const auto sys = temp.filePath(QStringLiteral("sys"));
        write(sys + QStringLiteral("/class/drm/card0/device/gpu_busy_percent"), QStringLiteral("20"));
        write(sys + QStringLiteral("/class/drm/card1/device/gpu_busy_percent"), QStringLiteral("80"));
        QDir().mkpath(sys + QStringLiteral("/class/drm/card2/device"));
        CpuSocSampler sampler({sys, temp.filePath(QStringLiteral("proc"))});
        const auto rows = sampler.sampleGpus({}); QCOMPARE(rows.size(), 3);
        QCOMPARE(rows[0].busyPercent, 20.0); QCOMPARE(rows[1].busyPercent, 80.0);
        QVERIFY(rows[0].identity != rows[1].identity); QVERIFY(!rows[2].available); QVERIFY(!rows[2].unavailableReason.isEmpty());
        QCOMPARE(sampler.sampleGpu({}).identity, rows[0].identity);
    }
    void diskRatesExcludeVirtualAndPartitionsAndReset() {
        QTemporaryDir temp; const auto sys = temp.filePath(QStringLiteral("sys")); const auto proc = temp.filePath(QStringLiteral("proc"));
        write(sys + QStringLiteral("/class/block/sda1/partition"), QStringLiteral("1"));
        auto stats = [](int n) { return QStringLiteral("8 0 sda %1 0 %2 %3 %1 0 %2 %3 0 0 0\n8 1 sda1 %1 0 %2 %3 %1 0 %2 %3 0 0 0\n253 0 dm-0 %1 0 %2 %3 %1 0 %2 %3 0 0 0\n").arg(n).arg(n * 2048).arg(n * 5); };
        write(proc + QStringLiteral("/diskstats"), stats(1));
        write(proc + QStringLiteral("/net/dev"), QStringLiteral("eth0: 0 0 3 4 0 0 0 0 0 0 5 6 0 0 0 0\n"));
        PsiMemorySampler sampler({sys, proc}); QVERIFY(!sampler.sample(1000).diskThroughputAvailable);
        write(proc + QStringLiteral("/diskstats"), stats(2)); const auto snap = sampler.sample(1000);
        QCOMPARE(snap.disks.size(), 3); QCOMPARE(snap.diskReadMibPerSec, 1.0);
        QCOMPARE(snap.disks.first().readIops, 1.0); QCOMPARE(snap.disks.first().averageRequestMs, 5.0);
        QVERIFY(snap.disks[1].isPartition); QVERIFY(!snap.disks[2].included);
        QCOMPARE(snap.networkInterfaces.first().rxErrors, quint64(3)); QCOMPARE(snap.networkInterfaces.first().txDropped, quint64(6));
        write(proc + QStringLiteral("/diskstats"), stats(0)); QVERIFY(!sampler.sample(1000).diskThroughputAvailable);
        write(proc + QStringLiteral("/diskstats"), stats(1)); QVERIFY(sampler.sample(1000).diskThroughputAvailable);
    }
    void batteriesWeightedUsbAndMissingVoltage() {
        QTemporaryDir temp; const auto sys = temp.filePath(QStringLiteral("sys"));
        const auto root = sys + QStringLiteral("/class/power_supply/");
        for (int n = 0; n < 2; ++n) {
            const auto battery = root + QStringLiteral("BAT%1/").arg(n);
            write(battery + QStringLiteral("type"), QStringLiteral("Battery"));
            write(battery + QStringLiteral("status"), QStringLiteral("Charging"));
            write(battery + QStringLiteral("energy_now"), n == 0 ? QStringLiteral("10000000") : QStringLiteral("60000000"));
            write(battery + QStringLiteral("energy_full"), n == 0 ? QStringLiteral("20000000") : QStringLiteral("80000000"));
            write(battery + QStringLiteral("energy_full_design"), n == 0 ? QStringLiteral("25000000") : QStringLiteral("100000000"));
            write(battery + QStringLiteral("power_now"), QStringLiteral("5000000"));
        }
        write(root + QStringLiteral("ucsi/type"), QStringLiteral("USB_PD")); write(root + QStringLiteral("ucsi/online"), QStringLiteral("1"));
        PowerFlowSampler sampler({sys, temp.filePath(QStringLiteral("proc"))}); const auto snap = sampler.sample(1000, -1, 0, 0);
        QCOMPARE(snap.batteries.size(), 2); QCOMPARE(snap.batteryPercent, 70.0); QCOMPARE(snap.batteryHealthPercent, 80.0);
        QVERIFY(snap.onAcPower); QVERIFY(snap.batteryStateAvailable); QCOMPARE(snap.batteryWatts, 10.0); QCOMPARE(snap.batteryTimeRemainingMinutes, 180);
        QVERIFY(QFile::remove(root + QStringLiteral("BAT1/energy_now")));
        write(root + QStringLiteral("BAT1/charge_now"), QStringLiteral("1000000"));
        const auto partial = sampler.sample(1000, -1, 0, 0); QVERIFY(!partial.batteryLevelAvailable); QVERIFY(!partial.batteries[1].energyAvailable);
        QCOMPARE(partial.batteries[1].voltageVolts, 0.0); QCOMPARE(partial.batteries[1].timeRemainingMinutes, -1);
    }
};
QTEST_GUILESS_MAIN(TestHardwareV09)
#include "test_hardware_v09.moc"
