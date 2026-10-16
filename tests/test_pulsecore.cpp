#include "causaldiagnosisengine.h"
#include "cpusocsampler.h"
#include "flightrecorder.h"
#include "powerflowsampler.h"
#include "psimemorysampler.h"
#include "sysfsreader.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
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
        QCOMPARE(snap.zramCompressionRatio, 4.0);
        QCOMPARE(snap.ramUsedPercent, 50.0);
        QCOMPARE(snap.cpuPsi.someAvg10, 2.5);
        QVERIFY(snap.responsivenessScore > 95.0);
    }

    void testCausalDiagnosisEngineVerdicts()
    {
        CpuSocSnapshot cpu;
        cpu.totalLoadPercent = 12.0;
        cpu.packageTempCelsius = 52.0;
        cpu.thermalHeadroomCelsius = 48.0;
        cpu.avgFreqMhz = 2400;

        GpuSnapshot gpu;
        PsiAndMemorySnapshot psi;
        psi.responsivenessScore = 99.6;
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
        QVERIFY(memBottleneck.causalChain.contains(QStringLiteral("ZRAM")));
        QVERIFY(memBottleneck.causalChain.contains(QStringLiteral("firefox-bin")));
        QVERIFY(memBottleneck.causalChain.contains(QStringLiteral("4096 MiB RSS")));

        // 2b. GPU Compute / Thermal Saturation verdict
        psi.memoryPsi.someAvg10 = 0.0;
        psi.memoryPsi.fullAvg10 = 0.0;
        gpu.available = true;
        gpu.driverName = QStringLiteral("amdgpu");
        gpu.busyPercent = 96.0;
        gpu.currentFreqMhz = 2400;
        gpu.tempCelsius = 82.0;
        const CausalVerdict gpuBottleneck = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(gpuBottleneck.severity, VerdictSeverity::Bottleneck);
        QCOMPARE(gpuBottleneck.primaryBottleneckDomain, QStringLiteral("Graphics & Display (GPU)"));

        // 3. Critical Thermal Throttling verdict with culprit attribution
        cpu.packageTempCelsius = 97.0;
        cpu.thermalHeadroomCelsius = 3.0;
        cpu.deltaThrottleEvents = 19;
        const CausalVerdict throttleVerdict = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(throttleVerdict.severity, VerdictSeverity::CriticalThrottle);
        QVERIFY(throttleVerdict.causalChain.contains(QStringLiteral("cc1plus")));
        QVERIFY(throttleVerdict.causalChain.contains(QStringLiteral("+19 throttle-events")));
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
