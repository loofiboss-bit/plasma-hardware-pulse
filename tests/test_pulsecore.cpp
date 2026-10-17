#include "causaldiagnosisengine.h"
#include "cpusocsampler.h"
#include "flightrecorder.h"
#include "powerflowsampler.h"
#include "psimemorysampler.h"
#include "sysfsreader.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

using namespace LoofiPulse;

namespace
{

void writeTextFile(const QString &path, const QString &content)
{
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    f.write(content.toUtf8());
    f.close();
}

} // namespace

class TestPulseCore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testHwmonDiscoveryAndVelocity()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString sysRoot = tempDir.filePath(QStringLiteral("sys"));
        const QString procRoot = tempDir.filePath(QStringLiteral("proc"));

        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/name"), QStringLiteral("coretemp\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/temp1_label"), QStringLiteral("Package id 0\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/temp1_input"), QStringLiteral("60000\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/temp1_max"), QStringLiteral("100000\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/temp1_crit"), QStringLiteral("100000\n"));

        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon1/name"), QStringLiteral("nvme\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon1/temp1_label"), QStringLiteral("Composite\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon1/temp1_input"), QStringLiteral("42000\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon1/temp1_max"), QStringLiteral("85000\n"));

        SysfsReader reader({sysRoot, procRoot});
        const QList<DomainSensorReading> firstSample = reader.sampleHwmonDomains(1000);
        QCOMPARE(firstSample.size(), 2);
        QCOMPARE(firstSample.at(0).domain, PhysicalDomain::CpuSoc);
        QCOMPARE(firstSample.at(0).currentCelsius, 60.0);
        QCOMPARE(firstSample.at(0).headroomCelsius, 40.0);

        // Increase CPU temp to 75°C after 1 second and verify positive thermal velocity
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/temp1_input"), QStringLiteral("75000\n"));
        const QList<DomainSensorReading> secondSample = reader.sampleHwmonDomains(1000);
        QCOMPARE(secondSample.at(0).currentCelsius, 75.0);
        QCOMPARE(secondSample.at(0).headroomCelsius, 25.0);
        QVERIFY(secondSample.at(0).velocityCelsiusPerSec > 4.0);

        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/temp1_input"), QStringLiteral("0\n"));
        const QList<DomainSensorReading> zeroSample = reader.sampleHwmonDomains(1000);
        QVERIFY(zeroSample.at(0).available);
        QCOMPARE(zeroSample.at(0).currentCelsius, 0.0);

        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/temp1_input"), QStringLiteral("unreadable\n"));
        const QList<DomainSensorReading> malformedSample = reader.sampleHwmonDomains(1000);
        QCOMPARE(malformedSample.size(), 1);
        QCOMPARE(malformedSample.first().domain, PhysicalDomain::StorageNvme);
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/temp1_input"), QStringLiteral("45000\n"));
        const QList<DomainSensorReading> recoveredSample = reader.sampleHwmonDomains(1000);
        QCOMPARE(recoveredSample.at(0).currentCelsius, 45.0);
        QCOMPARE(recoveredSample.at(0).velocityCelsiusPerSec, 0.0);
    }

    void testPsiAndZramCompression()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString sysRoot = tempDir.filePath(QStringLiteral("sys"));
        const QString procRoot = tempDir.filePath(QStringLiteral("proc"));

        writeTextFile(procRoot + QStringLiteral("/pressure/cpu"),
                      QStringLiteral("some avg10=2.50 avg60=1.20 avg300=0.50 total=100000\nfull avg10=0.00 avg60=0.00 avg300=0.00 total=0\n"));
        writeTextFile(procRoot + QStringLiteral("/pressure/memory"),
                      QStringLiteral("some avg10=0.00 avg60=0.00 avg300=0.00 total=50000\nfull avg10=0.00 avg60=0.00 avg300=0.00 total=10000\n"));
        writeTextFile(procRoot + QStringLiteral("/pressure/io"),
                      QStringLiteral("some avg10=1.00 avg60=0.50 avg300=0.20 total=80000\nfull avg10=0.50 avg60=0.25 avg300=0.10 total=40000\n"));

        writeTextFile(procRoot + QStringLiteral("/meminfo"),
                      QStringLiteral("MemTotal:       16000000 kB\nMemAvailable:    8000000 kB\nSwapTotal:       8000000 kB\nSwapFree:        7000000 kB\n"));

        // 1600 MB original compressed into 400 MB (4.0x ratio)
        writeTextFile(sysRoot + QStringLiteral("/block/zram0/mm_stat"),
                      QStringLiteral("1677721600 419430400 450000000 0 450000000 1000 200 100 200\n"));

        PsiMemorySampler sampler({sysRoot, procRoot});
        const PsiAndMemorySnapshot snap = sampler.sample(1000);

        QVERIFY(snap.zramActive);
        QVERIFY(snap.responsivenessAvailable);
        QCOMPARE(snap.zramCompressionRatio, 4.0);
        QCOMPARE(snap.ramUsedPercent, 50.0);
        QCOMPARE(snap.cpuPsi.someAvg10, 2.5);
        QVERIFY(snap.responsivenessScore > 95.0);
    }

    void testCausalDiagnosisEngineVerdicts()
    {
        CpuSocSnapshot cpu;
        cpu.totalLoadPercent = 12.0;
        cpu.loadAvailable = true;
        cpu.loadWarmedUp = true;
        cpu.packageTempCelsius = 52.0;
        cpu.temperatureAvailable = true;
        cpu.thermalLimitAvailable = true;
        cpu.thermalHeadroomCelsius = 48.0;
        cpu.avgFreqMhz = 2400;

        GpuSnapshot gpu;
        PsiAndMemorySnapshot psi;
        psi.responsivenessScore = 99.6;
        psi.responsivenessAvailable = true;
        psi.cpuPsi.someAvailable = true;
        psi.memoryPsi.someAvailable = true;
        psi.memoryPsi.fullAvailable = true;
        psi.ioPsi.someAvailable = true;
        psi.ioPsi.fullAvailable = true;
        psi.zramCompressionRatio = 4.2;

        PowerFlowSnapshot power;
        power.packageWatts = 8.5;
        power.activePowerProfile = QStringLiteral("balanced");

        QList<DomainSensorReading> domains;
        CulpritProcess culprit;
        culprit.pid = 4242;
        culprit.name = QStringLiteral("cc1plus");
        culprit.cpuPercent = 94.5;

        // 1. Nominal verdict
        const CausalVerdict nominal = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(nominal.severity, VerdictSeverity::Nominal);

        // 2. Memory PSI Bottleneck verdict with separate Memory RSS culprit attribution
        psi.memoryPsi.someAvg10 = 14.5;
        psi.memoryPsi.fullAvg10 = 4.2;
        CulpritProcess memHog;
        memHog.pid = 7777;
        memHog.name = QStringLiteral("firefox-bin");
        memHog.cpuPercent = 2.0;
        memHog.rssMegabytes = 4096.0;
        const CausalVerdict memBottleneck = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit, memHog);
        QCOMPARE(memBottleneck.severity, VerdictSeverity::Bottleneck);
        QVERIFY(memBottleneck.causalChain.contains(QStringLiteral("Memory pressure")));
        QVERIFY(memBottleneck.causalChain.contains(QStringLiteral("firefox-bin")));
        QVERIFY(memBottleneck.causalChain.contains(QStringLiteral("4096 MiB RSS")));

        // 2b. GPU Compute / Thermal Saturation verdict
        psi.memoryPsi.someAvg10 = 0.0;
        psi.memoryPsi.fullAvg10 = 0.0;
        gpu.available = true;
        gpu.busyAvailable = true;
        gpu.temperatureAvailable = true;
        gpu.driverName = QStringLiteral("amdgpu");
        gpu.busyPercent = 96.0;
        gpu.currentFreqMhz = 2400;
        gpu.tempCelsius = 82.0;
        const CausalVerdict gpuBottleneck = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(gpuBottleneck.severity, VerdictSeverity::Bottleneck);
        QCOMPARE(gpuBottleneck.primaryBottleneckDomain, QStringLiteral("Graphics"));

        // 3. Critical Thermal Throttling verdict with culprit attribution
        cpu.packageTempCelsius = 97.0;
        cpu.thermalHeadroomCelsius = 3.0;
        cpu.deltaThrottleEvents = 19;
        cpu.throttleCountersAvailable = true;
        const CausalVerdict throttleVerdict = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(throttleVerdict.severity, VerdictSeverity::CriticalThrottle);
        QCOMPARE(throttleVerdict.topProcess.name, QStringLiteral("cc1plus"));
        QVERIFY(!throttleVerdict.causalChain.contains(QStringLiteral("cc1plus")));
        QVERIFY(throttleVerdict.causalChain.contains(QStringLiteral("19 new throttle events")));
    }

    void testBatteryEtaAndHealthCalculation()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString sysRoot = tempDir.filePath(QStringLiteral("sys"));
        const QString procRoot = tempDir.filePath(QStringLiteral("proc"));

        writeTextFile(sysRoot + QStringLiteral("/class/power_supply/AC0/type"), QStringLiteral("Mains\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/power_supply/AC0/online"), QStringLiteral("0\n"));

        writeTextFile(sysRoot + QStringLiteral("/class/power_supply/BAT0/type"), QStringLiteral("Battery\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/power_supply/BAT0/status"), QStringLiteral("Discharging\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/power_supply/BAT0/capacity"), QStringLiteral("50\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/power_supply/BAT0/voltage_now"), QStringLiteral("12000000\n"));
        // 15W discharge, 30Wh remaining -> 120 minutes remaining; 45Wh full / 50Wh design -> 90% health
        writeTextFile(sysRoot + QStringLiteral("/class/power_supply/BAT0/power_now"), QStringLiteral("15000000\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/power_supply/BAT0/energy_now"), QStringLiteral("30000000\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/power_supply/BAT0/energy_full"), QStringLiteral("45000000\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/power_supply/BAT0/energy_full_design"), QStringLiteral("50000000\n"));

        PowerFlowSampler sampler({sysRoot, procRoot});
        const PowerFlowSnapshot snap = sampler.sample(1000, 25.0, 2000, 4000);
        QVERIFY(!snap.onAcPower);
        QVERIFY(snap.batteryPresent);
        QCOMPARE(snap.batteryWatts, 15.0);
        QCOMPARE(snap.batteryTimeRemainingMinutes, 120);
        QCOMPARE(snap.batteryHealthPercent, 90.0);
    }

    void testBatteryOnlyDischargingAndZeroPower()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString battery = tempDir.filePath(QStringLiteral("sys/class/power_supply/BAT0"));
        writeTextFile(battery + QStringLiteral("/type"), QStringLiteral("Battery\n"));
        writeTextFile(battery + QStringLiteral("/status"), QStringLiteral("Discharging\n"));
        writeTextFile(battery + QStringLiteral("/power_now"), QStringLiteral("0\n"));
        PowerFlowSampler sampler({tempDir.filePath(QStringLiteral("sys")), tempDir.filePath(QStringLiteral("proc"))});
        const PowerFlowSnapshot sample = sampler.sample(1000, -1.0, 0, 0);
        QVERIFY(sample.batteryStateAvailable);
        QVERIFY(!sample.onAcPower);
        QVERIFY(sample.batteryPowerAvailable);
        QCOMPARE(sample.batteryWatts, 0.0);
    }

    void testReadableEmptyZramIsAvailableWithoutRatio()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        writeTextFile(tempDir.filePath(QStringLiteral("sys/block/zram0/mm_stat")), QStringLiteral("0 0 0 0 0 0 0 0 0\n"));
        PsiMemorySampler sampler({tempDir.filePath(QStringLiteral("sys")), tempDir.filePath(QStringLiteral("proc"))});
        const PsiAndMemorySnapshot sample = sampler.sample(1000);
        QVERIFY(sample.zramAvailable);
        QVERIFY(!sample.zramCompressionAvailable);
        QCOMPARE(sample.zramOrigBytes, quint64(0));
    }

    void testThrottleSensorDisappearanceDoesNotCreateEvent()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString sysRoot = tempDir.filePath(QStringLiteral("sys"));
        const QString procRoot = tempDir.filePath(QStringLiteral("proc"));
        const QString corePath = sysRoot + QStringLiteral("/devices/system/cpu/cpu0/thermal_throttle/core_throttle_count");
        const QString packagePath = sysRoot + QStringLiteral("/devices/system/cpu/cpu0/thermal_throttle/package_throttle_count");
        writeTextFile(procRoot + QStringLiteral("/stat"), QStringLiteral("cpu 1 0 1 8 0 0 0 0 0 0\ncpu0 1 0 1 8 0 0 0 0 0 0\n"));
        writeTextFile(corePath, QStringLiteral("10\n"));
        writeTextFile(packagePath, QStringLiteral("20\n"));
        CpuSocSampler sampler({sysRoot, procRoot});
        sampler.sampleCpu({}, 1000);
        QVERIFY(QFile::remove(packagePath));
        QCOMPARE(sampler.sampleCpu({}, 1000).deltaThrottleEvents, qint64(0));
        writeTextFile(packagePath, QStringLiteral("20\n"));
        QCOMPARE(sampler.sampleCpu({}, 1000).deltaThrottleEvents, qint64(0));
        writeTextFile(packagePath, QStringLiteral("21\n"));
        QCOMPARE(sampler.sampleCpu({}, 1000).deltaThrottleEvents, qint64(1));

        // A larger unchanged core total must not conceal a new package event.
        writeTextFile(corePath, QStringLiteral("30\n"));
        writeTextFile(packagePath, QStringLiteral("10\n"));
        QCOMPARE(sampler.sampleCpu({}, 1000).deltaThrottleEvents, qint64(0));
        writeTextFile(packagePath, QStringLiteral("11\n"));
        QCOMPARE(sampler.sampleCpu({}, 1000).deltaThrottleEvents, qint64(1));
    }

    void testFlightRecorderSpikeLookup()
    {
        FlightRecorder recorder(5);
        for (int i = 0; i < 5; ++i) {
            FlightSample s;
            s.timestampMs = 1000 * (i + 1);
            s.cpuTempCelsius = 50.0 + i * 5.0;
            s.responsivenessScore = 99.0;
            s.culpritName = QStringLiteral("worker_%1").arg(i);
            recorder.appendSample(s);
        }

        // Inject spike at index 2
        FlightRecorder rec2(10);
        FlightSample s0; s0.cpuTempCelsius = 55.0; s0.responsivenessScore = 99.0;
        FlightSample s1; s1.cpuTempCelsius = 94.0; s1.responsivenessScore = 72.0; s1.culpritName = QStringLiteral("rustc"); s1.culpritPid = 1234; s1.throttled = true;
        FlightSample s2; s2.cpuTempCelsius = 62.0; s2.responsivenessScore = 98.0;
        for (FlightSample *sample : {&s0, &s1, &s2}) {
            sample->metricQuality.insert(QStringLiteral("cpuTempAvailable"), true);
            sample->metricQuality.insert(QStringLiteral("psiAvailable"), true);
        }
        rec2.appendSample(s0);
        rec2.appendSample(s1);
        rec2.appendSample(s2);

        QCOMPARE(rec2.peakTemperatureIndex(), 1);
        QCOMPARE(rec2.worstResponsivenessIndex(), 1);
        QCOMPARE(rec2.sampleAt(1).culpritName, QStringLiteral("rustc"));

        const QString md = rec2.exportSummaryMarkdown();
        QVERIFY(md.contains(QStringLiteral("rustc")));
        QVERIFY(md.contains(QStringLiteral("Peak Thermal Spike")));

        const QString json = rec2.exportSamplesJson();
        QVERIFY(json.contains(QStringLiteral("\"culpritName\": \"rustc\"")));
    }

    void testMissingPsiIsUnknownAndRecovers()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString sysRoot = tempDir.filePath(QStringLiteral("sys"));
        const QString procRoot = tempDir.filePath(QStringLiteral("proc"));
        writeTextFile(procRoot + QStringLiteral("/meminfo"),
                      QStringLiteral("MemTotal: 1000 kB\nMemAvailable: 900 kB\n"));

        PsiMemorySampler sampler({sysRoot, procRoot});
        const PsiAndMemorySnapshot missing = sampler.sample(1000);
        QVERIFY(!missing.responsivenessAvailable);
        QCOMPARE(missing.responsivenessScore, 0.0);

        CpuSocSnapshot cpu;
        cpu.loadAvailable = true;
        cpu.loadWarmedUp = true;
        cpu.temperatureAvailable = true;
        const CausalVerdict unknown = CausalDiagnosisEngine::evaluate(cpu, {}, missing, {}, {}, {});
        QCOMPARE(unknown.severity, VerdictSeverity::Unknown);
        QVERIFY(unknown.headline.contains(QStringLiteral("unavailable")));

        const QString psi = QStringLiteral("some avg10=0.00 avg60=0.00 avg300=0.00 total=0\n");
        writeTextFile(procRoot + QStringLiteral("/pressure/cpu"), psi);
        writeTextFile(procRoot + QStringLiteral("/pressure/memory"), psi + QStringLiteral("full avg10=0.00 avg60=0.00 avg300=0.00 total=0\n"));
        writeTextFile(procRoot + QStringLiteral("/pressure/io"), psi + QStringLiteral("full avg10=0.00 avg60=0.00 avg300=0.00 total=0\n"));
        const PsiAndMemorySnapshot recovered = sampler.sample(1000);
        QVERIFY(recovered.responsivenessAvailable);
        QCOMPARE(recovered.responsivenessScore, 100.0);
        QCOMPARE(recovered.cpuPsi.someAvg10, 0.0);
    }

    void testMalformedPsiIsUnavailable()
    {
        const PsiMetric malformed = PsiMemorySampler::parsePsiContent(
            QStringLiteral("some avg10=nan avg60=1 avg300=0 total=broken\n"), 100, 1000);
        QVERIFY(!malformed.available);
        QCOMPARE(malformed.someAvg10, 0.0);
        QCOMPARE(malformed.deltaStallMsPerSec, 0.0);

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString procRoot = tempDir.filePath(QStringLiteral("proc"));
        const QString some = QStringLiteral("some avg10=0.00 avg60=0.00 avg300=0.00 total=0\n");
        writeTextFile(procRoot + QStringLiteral("/pressure/cpu"), some);
        writeTextFile(procRoot + QStringLiteral("/pressure/memory"), some);
        writeTextFile(procRoot + QStringLiteral("/pressure/io"), some);
        writeTextFile(procRoot + QStringLiteral("/meminfo"), QStringLiteral("MemTotal: 1000 kB\nMemAvailable: 900 kB\n"));
        const PsiAndMemorySnapshot missingFull = PsiMemorySampler({tempDir.filePath(QStringLiteral("sys")), procRoot}).sample(1000);
        QVERIFY(!missingFull.responsivenessAvailable);
        CpuSocSnapshot cpu;
        cpu.loadAvailable = true;
        cpu.loadWarmedUp = true;
        cpu.temperatureAvailable = true;
        const CausalVerdict verdict = CausalDiagnosisEngine::evaluate(cpu, {}, missingFull, {}, {}, {});
        QCOMPARE(verdict.severity, VerdictSeverity::Unknown);
    }

    void testCpuCountersResetAfterMissingDataAndRollback()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString sysRoot = tempDir.filePath(QStringLiteral("sys"));
        const QString procRoot = tempDir.filePath(QStringLiteral("proc"));
        const QString statPath = procRoot + QStringLiteral("/stat");
        const auto stat = [](quint64 user, quint64 system, quint64 idle) {
            return QStringLiteral("cpu %1 0 %2 %3 0 0 0 0 0 0\ncpu0 %1 0 %2 %3 0 0 0 0 0 0\n")
                .arg(user).arg(system).arg(idle);
        };

        CpuSocSampler sampler({sysRoot, procRoot});
        writeTextFile(statPath, stat(10, 10, 80));
        const CpuSocSnapshot baseline = sampler.sampleCpu({}, 1000);
        QVERIFY(baseline.loadAvailable);
        QVERIFY(!baseline.loadWarmedUp);

        writeTextFile(statPath, stat(20, 20, 160));
        const CpuSocSnapshot warmed = sampler.sampleCpu({}, 1000);
        QVERIFY(warmed.loadWarmedUp);
        QVERIFY(qAbs(warmed.totalLoadPercent - 20.0) < 0.01);
        QVERIFY(warmed.cores.first().loadAvailable);

        QVERIFY(QFile::remove(statPath));
        const CpuSocSnapshot missing = sampler.sampleCpu({}, 1000);
        QVERIFY(!missing.loadAvailable);
        QVERIFY(!missing.loadWarmedUp);

        writeTextFile(statPath, stat(1, 1, 8));
        const CpuSocSnapshot recoveredBaseline = sampler.sampleCpu({}, 1000);
        QVERIFY(recoveredBaseline.loadAvailable);
        QVERIFY(!recoveredBaseline.loadWarmedUp);

        writeTextFile(statPath, stat(2, 2, 16));
        const CpuSocSnapshot recovered = sampler.sampleCpu({}, 1000);
        QVERIFY(recovered.loadWarmedUp);
        QVERIFY(qAbs(recovered.totalLoadPercent - 20.0) < 0.01);

        // Active counters can reset while total ticks continue to increase.
        writeTextFile(statPath, stat(0, 0, 200));
        const CpuSocSnapshot rolledBack = sampler.sampleCpu({}, 1000);
        QVERIFY(!rolledBack.loadWarmedUp);
        QVERIFY(!rolledBack.cores.first().loadAvailable);
        QCOMPARE(rolledBack.cores.first().loadPercent, 0.0);
    }

    void testGpuMetricsStayWithTheirDeviceAndUnknownBusy()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString sysRoot = tempDir.filePath(QStringLiteral("sys"));
        const QString procRoot = tempDir.filePath(QStringLiteral("proc"));
        const QString card0Device = sysRoot + QStringLiteral("/devices/pci0000:00/0000:00:02.0");
        const QString card1Device = sysRoot + QStringLiteral("/devices/pci0000:00/0000:03.0");
        writeTextFile(card0Device + QStringLiteral("/gt_act_freq_mhz"), QStringLiteral("300\n"));
        writeTextFile(card0Device + QStringLiteral("/gpu_busy_percent"), QStringLiteral("0\n"));
        writeTextFile(card0Device + QStringLiteral("/hwmon/hwmon0/name"), QStringLiteral("amdgpu\n"));
        writeTextFile(card0Device + QStringLiteral("/hwmon/hwmon0/temp1_label"), QStringLiteral("GPU core\n"));
        writeTextFile(card0Device + QStringLiteral("/hwmon/hwmon0/temp1_input"), QStringLiteral("65000\n"));
        writeTextFile(card1Device + QStringLiteral("/gt_act_freq_mhz"), QStringLiteral("2000\n"));
        writeTextFile(card1Device + QStringLiteral("/gpu_busy_percent"), QStringLiteral("95\n"));
        writeTextFile(card1Device + QStringLiteral("/hwmon/hwmon0/name"), QStringLiteral("amdgpu\n"));
        writeTextFile(card1Device + QStringLiteral("/hwmon/hwmon0/temp1_label"), QStringLiteral("GPU core\n"));
        writeTextFile(card1Device + QStringLiteral("/hwmon/hwmon0/temp1_input"), QStringLiteral("100000\n"));

        const QString card0 = sysRoot + QStringLiteral("/class/drm/card0");
        const QString card1 = sysRoot + QStringLiteral("/class/drm/card1");
        QDir().mkpath(sysRoot + QStringLiteral("/class/hwmon"));
        QDir().mkpath(card0);
        QDir().mkpath(card1);
        writeTextFile(card0 + QStringLiteral("/gt_act_freq_mhz"), QStringLiteral("300\n"));
        writeTextFile(card1 + QStringLiteral("/gt_act_freq_mhz"), QStringLiteral("2000\n"));
        QVERIFY(QFile::link(card0Device, card0 + QStringLiteral("/device")));
        QVERIFY(QFile::link(card1Device, card1 + QStringLiteral("/device")));
        QVERIFY(QFile::link(card0Device + QStringLiteral("/hwmon/hwmon0"), sysRoot + QStringLiteral("/class/hwmon/hwmon0")));
        QVERIFY(QFile::link(card1Device + QStringLiteral("/hwmon/hwmon0"), sysRoot + QStringLiteral("/class/hwmon/hwmon1")));

        SysfsReader reader({sysRoot, procRoot});
        const QList<DomainSensorReading> sensors = reader.sampleHwmonDomains(1000);
        QCOMPARE(sensors.size(), 2);
        CpuSocSampler sampler({sysRoot, procRoot});
        const GpuSnapshot gpu = sampler.sampleGpu(sensors);
        QVERIFY(gpu.available);
        QVERIFY(gpu.busyAvailable);
        QVERIFY(gpu.temperatureAvailable);
        QCOMPARE(gpu.busyPercent, 0.0);
        QCOMPARE(gpu.tempCelsius, 65.0);
        QCOMPARE(gpu.currentFreqMhz, 300);
        writeTextFile(card0Device + QStringLiteral("/hwmon/hwmon0/power1_average"), QStringLiteral("0\n"));
        const GpuSnapshot zeroPower = sampler.sampleGpu(sensors);
        QVERIFY(zeroPower.powerAvailable);
        QCOMPARE(zeroPower.powerWatts, 0.0);

        QFile::remove(card0Device + QStringLiteral("/gpu_busy_percent"));
        QFile::remove(card1Device + QStringLiteral("/gpu_busy_percent"));
        const GpuSnapshot sampledWithoutBusy = sampler.sampleGpu({});
        QVERIFY(!sampledWithoutBusy.busyAvailable);
        QVERIFY(!sampledWithoutBusy.temperatureAvailable);
    }

    void testHotStorageTemperatureDoesNotClaimIoBottleneck()
    {
        CpuSocSnapshot cpu;
        cpu.loadAvailable = true;
        cpu.loadWarmedUp = true;
        cpu.temperatureAvailable = true;
        PsiAndMemorySnapshot psi;
        psi.responsivenessAvailable = true;
        psi.ioPsi.someAvailable = true;
        psi.ioPsi.fullAvailable = true;
        QList<DomainSensorReading> domains;
        DomainSensorReading nvme;
        nvme.available = true;
        nvme.domain = PhysicalDomain::StorageNvme;
        nvme.currentCelsius = 78.0;
        domains.append(nvme);

        const CausalVerdict verdict = CausalDiagnosisEngine::evaluate(cpu, {}, psi, {}, domains, {});
        QCOMPARE(verdict.severity, VerdictSeverity::Elevated);
        QVERIFY(verdict.headline.contains(QStringLiteral("NVMe temperature")));
        QVERIFY(!verdict.headline.contains(QStringLiteral("I/O")));
    }

    void testThermalRiskIsNotReportedAsActiveThrottling()
    {
        CpuSocSnapshot cpu;
        cpu.loadAvailable = true;
        cpu.loadWarmedUp = true;
        cpu.temperatureAvailable = true;
        cpu.thermalLimitAvailable = true;
        cpu.packageTempCelsius = 96.0;
        cpu.thermalHeadroomCelsius = 4.0;
        cpu.throttleCountersAvailable = true;
        PsiAndMemorySnapshot psi;
        psi.responsivenessAvailable = true;
        psi.cpuPsi.someAvailable = true;
        psi.memoryPsi.someAvailable = true;
        psi.ioPsi.someAvailable = true;

        const CausalVerdict verdict = CausalDiagnosisEngine::evaluate(cpu, {}, psi, {}, {}, {});
        QCOMPARE(verdict.severity, VerdictSeverity::Elevated);
        QVERIFY(verdict.headline.contains(QStringLiteral("close to its reported thermal limit")));
    }

    void testRaplCounterWrapUsesHardwareRange()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString sysRoot = tempDir.filePath(QStringLiteral("sys"));
        const QString rapl = sysRoot + QStringLiteral("/class/powercap/intel-rapl:0");
        writeTextFile(rapl + QStringLiteral("/max_energy_range_uj"), QStringLiteral("1000\n"));
        writeTextFile(rapl + QStringLiteral("/energy_uj"), QStringLiteral("900\n"));
        PowerFlowSampler sampler({sysRoot, tempDir.filePath(QStringLiteral("proc"))});
        sampler.sample(1000, 0.0, 0, 0);
        writeTextFile(rapl + QStringLiteral("/energy_uj"), QStringLiteral("100\n"));
        const PowerFlowSnapshot wrapped = sampler.sample(1000, 0.0, 0, 0);
        QVERIFY(wrapped.raplExactAvailable);
        QVERIFY(wrapped.packagePowerAvailable);
        QVERIFY(qAbs(wrapped.packageWatts - 0.0002) < 0.000001);
    }

    void testRaplUnavailableReadingResetsInterval()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString rapl = tempDir.filePath(QStringLiteral("sys/class/powercap/intel-rapl:0"));
        writeTextFile(rapl + QStringLiteral("/max_energy_range_uj"), QStringLiteral("100000000\n"));
        writeTextFile(rapl + QStringLiteral("/energy_uj"), QStringLiteral("10000000\n"));
        PowerFlowSampler sampler({tempDir.filePath(QStringLiteral("sys")), tempDir.filePath(QStringLiteral("proc"))});
        sampler.sample(1000, -1.0, 0, 0);

        QVERIFY(QFile::remove(rapl + QStringLiteral("/energy_uj")));
        const PowerFlowSnapshot missing = sampler.sample(1000, -1.0, 0, 0);
        QVERIFY(!missing.raplExactAvailable);
        QVERIFY(!missing.packagePowerAvailable);

        writeTextFile(rapl + QStringLiteral("/energy_uj"), QStringLiteral("30000000\n"));
        const PowerFlowSnapshot recoveredBaseline = sampler.sample(1000, -1.0, 0, 0);
        QVERIFY(!recoveredBaseline.raplExactAvailable);
        QVERIFY(!recoveredBaseline.packagePowerAvailable);

        writeTextFile(rapl + QStringLiteral("/energy_uj"), QStringLiteral("31000000\n"));
        const PowerFlowSnapshot recovered = sampler.sample(1000, -1.0, 0, 0);
        QVERIFY(recovered.raplExactAvailable);
        QVERIFY(qAbs(recovered.packageWatts - 1.0) < 0.000001);
    }

    void testFlightRecorderUsesRetentionWindowAndStableIds()
    {
        FlightRecorder recorder(1200);
        recorder.setRetentionMs(3000);
        quint64 secondSampleId = 0;
        for (int i = 1; i <= 5; ++i) {
            FlightSample sample;
            sample.timestampMs = i * 1000;
            sample.monotonicMs = i * 1000;
            if (i == 2) secondSampleId = recorder.appendSample(sample);
            else recorder.appendSample(sample);
        }
        QCOMPARE(recorder.count(), 4);
        QCOMPARE(recorder.indexOfSample(secondSampleId), 0);

        FlightSample later;
        later.timestampMs = 6000;
        later.monotonicMs = 6000;
        recorder.appendSample(later);
        QCOMPARE(recorder.indexOfSample(secondSampleId), -1);
        QCOMPARE(recorder.sampleAt(0).timestampMs, qint64(3000));
    }

    void testFlightRecorderFiveMinuteWindowAcrossIntervals()
    {
        for (const int intervalMs : {500, 1000, 2000}) {
            FlightRecorder recorder;
            quint64 initialId = 0;
            qint64 lastTimeMs = 0;
            for (qint64 timeMs = 1000; timeMs <= 302000; timeMs += intervalMs) {
                FlightSample sample;
                sample.monotonicMs = timeMs;
                sample.timestampMs = 1700000000000LL + timeMs;
                const quint64 id = recorder.appendSample(sample);
                if (timeMs == 1000) initialId = id;
                lastTimeMs = timeMs;
            }
            QVERIFY(recorder.count() > 0);
            const qint64 cutoff = lastTimeMs - 5 * 60 * 1000;
            const qint64 expectedOldest = 1000 + ((qMax<qint64>(cutoff - 1000, 0) + intervalMs - 1) / intervalMs) * intervalMs;
            QCOMPARE(recorder.sampleAt(0).monotonicMs, expectedOldest);
            QCOMPARE(recorder.indexOfSample(initialId), expectedOldest == 1000 ? 0 : -1);
        }

        FlightRecorder changedInterval;
        for (qint64 timeMs = 1000; timeMs <= 150000; timeMs += 500) {
            FlightSample sample;
            sample.monotonicMs = timeMs;
            changedInterval.appendSample(sample);
        }
        for (qint64 timeMs = 152000; timeMs <= 304000; timeMs += 2000) {
            FlightSample sample;
            sample.monotonicMs = timeMs;
            changedInterval.appendSample(sample);
        }
        QCOMPARE(changedInterval.sampleAt(0).monotonicMs, qint64(4000));

        // Pauses and suspend advance CLOCK_BOOTTIME even when no samples arrive.
        FlightSample afterPause;
        afterPause.monotonicMs = 620000;
        afterPause.elapsedSincePreviousMs = 316000;
        changedInterval.appendSample(afterPause);
        QCOMPARE(changedInterval.count(), 1);
        QCOMPARE(changedInterval.sampleAt(0).elapsedSincePreviousMs, qint64(316000));
    }

    void testHistoryAndReportsSkipUnavailableValues()
    {
        FlightRecorder recorder;
        FlightSample validZero;
        validZero.timestampMs = 1000;
        validZero.monotonicMs = 1000;
        validZero.cpuTempCelsius = 0.0;
        validZero.responsivenessScore = 0.0;
        validZero.metricQuality.insert(QStringLiteral("cpuTempAvailable"), true);
        validZero.metricQuality.insert(QStringLiteral("psiAvailable"), true);
        recorder.appendSample(validZero);

        FlightSample missing;
        missing.timestampMs = 5000;
        missing.monotonicMs = 5000;
        missing.cpuTempCelsius = 100.0;
        missing.responsivenessScore = 1.0;
        missing.elapsedSincePreviousMs = 4000;
        recorder.appendSample(missing);

        QCOMPARE(recorder.peakTemperatureIndex(), 0);
        QCOMPARE(recorder.worstResponsivenessIndex(), 0);
        const QJsonArray exported = QJsonDocument::fromJson(recorder.exportSamplesJson().toUtf8()).array();
        QCOMPARE(exported.size(), 2);
        QCOMPARE(exported.at(0).toObject().value(QStringLiteral("cpuTempCelsius")).toDouble(), 0.0);
        QCOMPARE(exported.at(0).toObject().value(QStringLiteral("responsivenessScore")).toDouble(), 0.0);
        QVERIFY(exported.at(1).toObject().value(QStringLiteral("cpuTempCelsius")).isNull());
        QVERIFY(exported.at(1).toObject().value(QStringLiteral("responsivenessScore")).isNull());
        QCOMPARE(exported.at(1).toObject().value(QStringLiteral("recordedPeriodMs")).toInteger(), qint64(4000));
        const QString markdown = recorder.exportSummaryMarkdown();
        QVERIFY(markdown.contains(QStringLiteral("Unavailable")));
        QVERIFY(markdown.contains(QStringLiteral("Concurrent load observation")));
    }

    void testJsonReportHasVersionQualityAndRecordedChannels()
    {
        FlightRecorder recorder;
        FlightSample sample;
        sample.timestampMs = 1000;
        sample.monotonicMs = 200;
        sample.cpuTempCelsius = 0.0;
        sample.gpuTempCelsius = 73.0;
        sample.nvmeTempCelsius = 44.0;
        sample.psiCpuSome10 = 0.0;
        sample.responsivenessScore = 0.0;
        sample.metricQuality.insert(QStringLiteral("cpuTempAvailable"), true);
        sample.metricQuality.insert(QStringLiteral("gpuTempAvailable"), true);
        sample.metricQuality.insert(QStringLiteral("nvmeTempAvailable"), true);
        sample.metricQuality.insert(QStringLiteral("psiAvailable"), true);
        recorder.appendSample(sample);

        const QJsonDocument document = QJsonDocument::fromJson(recorder.exportSamplesJson().toUtf8());
        QVERIFY(document.isArray());
        const QJsonObject exported = document.array().first().toObject();
        QCOMPARE(exported.value(QStringLiteral("schemaVersion")).toInt(), 1);
        QCOMPARE(exported.value(QStringLiteral("responsivenessScore")).toDouble(), 0.0);
        QCOMPARE(exported.value(QStringLiteral("cpuTempCelsius")).toDouble(), 0.0);
        QCOMPARE(exported.value(QStringLiteral("gpuTempCelsius")).toDouble(), 73.0);
        QCOMPARE(exported.value(QStringLiteral("nvmeTempCelsius")).toDouble(), 44.0);
        QVERIFY(exported.value(QStringLiteral("metricQuality")).isObject());
        const QJsonObject metaObj = exported.value(QStringLiteral("metricMetadata")).toObject();
        QCOMPARE(metaObj.value(QStringLiteral("cpuTemperature")).toObject().value(QStringLiteral("state")).toString(), QStringLiteral("available"));
        QCOMPARE(metaObj.value(QStringLiteral("pressureIndex")).toObject().value(QStringLiteral("state")).toString(), QStringLiteral("calculated"));
        const QString md = recorder.exportSummaryMarkdown();
        QVERIFY(md.contains(QStringLiteral("## Data quality at latest sample")));
        QVERIFY(md.contains(QStringLiteral("**cpuTemperature**: available")));
    }

    void testVerdictStabilityAndImmediateUnknown()
    {
        VerdictStabilizer stabilizer;
        CausalVerdict nominal;
        nominal.severity = VerdictSeverity::Nominal;
        nominal.headline = QStringLiteral("Normal");
        QCOMPARE(stabilizer.update(nominal, 0).severity, VerdictSeverity::Unknown);
        QCOMPARE(stabilizer.current().headline, QStringLiteral("Collecting initial readings"));
        QCOMPARE(stabilizer.update(nominal, 4999).severity, VerdictSeverity::Unknown);
        QCOMPARE(stabilizer.update(nominal, 5000).severity, VerdictSeverity::Nominal);

        CausalVerdict risk;
        risk.severity = VerdictSeverity::Bottleneck;
        risk.headline = QStringLiteral("Pressure");
        QCOMPARE(stabilizer.update(risk, 5100).severity, VerdictSeverity::Nominal);
        QCOMPARE(stabilizer.update(risk, 8099).severity, VerdictSeverity::Nominal);
        QCOMPARE(stabilizer.update(risk, 8100).severity, VerdictSeverity::Bottleneck);

        CausalVerdict unknown;
        unknown.severity = VerdictSeverity::Unknown;
        QCOMPARE(stabilizer.update(unknown, 8200).severity, VerdictSeverity::Unknown);
        QCOMPARE(stabilizer.update(risk, 8300).severity, VerdictSeverity::Unknown);
        QCOMPARE(stabilizer.update(risk, 11299).severity, VerdictSeverity::Unknown);
        QCOMPARE(stabilizer.update(risk, 11300).severity, VerdictSeverity::Bottleneck);
        QCOMPARE(stabilizer.update(nominal, 11400).severity, VerdictSeverity::Bottleneck);
        QCOMPARE(stabilizer.update(nominal, 16399).severity, VerdictSeverity::Bottleneck);
        QCOMPARE(stabilizer.update(nominal, 16400).severity, VerdictSeverity::Nominal);
    }

    void testLiveHostTelemetrySanity()
    {
        SysfsReader reader;
        const QList<DomainSensorReading> domains = reader.sampleHwmonDomains(1000);
        QVERIFY(!domains.isEmpty());

        CpuSocSampler cpuSampler;
        const CpuSocSnapshot cpu = cpuSampler.sampleCpu(domains);
        QVERIFY(!cpu.cores.isEmpty());
        QVERIFY(cpu.packageTempCelsius > 10.0);

        const ProcessRadarSnapshot radar = cpuSampler.sampleProcessRadar(5);
        QVERIFY(!radar.topProcesses.isEmpty());
        QVERIFY(radar.topMemoryProcess.rssMegabytes > 0.0);

        PsiMemorySampler psiSampler;
        const PsiAndMemorySnapshot psi = psiSampler.sample(1000);
        QVERIFY(psi.ramTotalBytes > 0ULL);
        QVERIFY(psi.responsivenessScore >= 0.0 && psi.responsivenessScore <= 100.0);
    }
};

QTEST_MAIN(TestPulseCore)
#include "test_pulsecore.moc"
