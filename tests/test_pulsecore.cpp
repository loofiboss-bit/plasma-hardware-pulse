#include "causaldiagnosisengine.h"
#include "cpusocsampler.h"
#include "flightrecorder.h"
#include "kernellogreader.h"
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
    void testGpuTemperatureRiskAcrossDevices()
    {
        CpuSocSnapshot cpu;
        cpu.loadAvailable = cpu.loadWarmedUp = cpu.temperatureAvailable = true;
        PsiAndMemorySnapshot psi;
        psi.responsivenessAvailable = true;
        GpuSnapshot idle;
        idle.identity = QStringLiteral("integrated");
        idle.temperatureAvailable = idle.busyAvailable = true;
        idle.tempCelsius = 45.0;
        GpuSnapshot hot = idle;
        hot.identity = QStringLiteral("discrete");
        hot.temperatureSensorId = QStringLiteral("gpu/edge");
        hot.temperatureSensorLabel = QStringLiteral("edge");
        hot.tempCelsius = 95.0;
        for (const double busy : {0.0, 20.0}) {
            hot.busyPercent = busy;
            const auto verdict = CausalDiagnosisEngine::evaluateDevices(cpu, {idle, hot}, psi, {}, {}, {});
            QCOMPARE(verdict.ruleId, QStringLiteral("gpu.temperature"));
            QCOMPARE(verdict.severity, VerdictSeverity::Elevated);
            QVERIFY(verdict.causalChain.contains(hot.identity));
            QVERIFY(verdict.causalChain.contains(hot.temperatureSensorId));
        }
        hot.busyPercent = 95.0;
        const auto pressure = CausalDiagnosisEngine::evaluateDevices(cpu, {idle, hot}, psi, {}, {}, {});
        QCOMPARE(pressure.ruleId, QStringLiteral("gpu.load-temperature"));
        QCOMPARE(pressure.severity, VerdictSeverity::Bottleneck);
        QCOMPARE(CausalDiagnosisEngine::evaluateDevices(cpu, {hot, idle}, psi, {}, {}, {}).causalChain, pressure.causalChain);
        hot.busyAvailable = false;
        QCOMPARE(CausalDiagnosisEngine::evaluateDevices(cpu, {idle, hot}, psi, {}, {}, {}).severity, VerdictSeverity::Elevated);
        QCOMPARE(CausalDiagnosisEngine::evaluateDevices(cpu, {idle}, psi, {}, {}, {}).severity, VerdictSeverity::Nominal);
        hot.temperatureAvailable = false;
        hot.busyAvailable = true;
        PowerFlowSnapshot power;
        power.activePowerProfile = QStringLiteral("power-saver");
        const auto saver = CausalDiagnosisEngine::evaluateDevices(cpu, {idle, hot}, psi, power, {}, {});
        QCOMPARE(saver.ruleId, QStringLiteral("power.saver-workload"));
        QVERIFY(saver.causalChain.contains(hot.identity));
    }

    void testRaplHighPackagePowerPreserved()
    {
        QTemporaryDir temp;
        const QString rapl = temp.filePath(QStringLiteral("sys/class/powercap/intel-rapl:0"));
        writeTextFile(rapl + QStringLiteral("/energy_uj"), QStringLiteral("100000000"));
        writeTextFile(rapl + QStringLiteral("/constraint_0_power_limit_uw"), QStringLiteral("300000000"));
        PowerFlowSampler sampler({temp.filePath(QStringLiteral("sys")), temp.filePath(QStringLiteral("proc"))});
        QVERIFY(!sampler.sample(1000, -1, 0, 0).raplExactAvailable);
        writeTextFile(rapl + QStringLiteral("/energy_uj"), QStringLiteral("400000000"));
        const auto power = sampler.sample(1000, -1, 0, 0);
        QVERIFY(power.raplExactAvailable);
        QCOMPARE(power.packageWatts, 300.0);
        CpuSocSnapshot cpu;
        cpu.loadAvailable = cpu.loadWarmedUp = cpu.temperatureAvailable = true;
        cpu.totalLoadPercent = 95;
        QCOMPARE(CausalDiagnosisEngine::evaluate(cpu, {}, {}, power, {}, {}).ruleId, QStringLiteral("cpu.sustained-power-limit"));
    }

    void testPsiAvg60RequiredForAvailability()
    {
        for (const auto &avg60 : {QString(), QStringLiteral("avg60=nan "), QStringLiteral("avg60=broken "), QStringLiteral("avg60=101 ")}) {
            const auto metric = PsiMemorySampler::parsePsiContent(
                QStringLiteral("some avg10=0 %1total=200\nfull avg10=0 %1total=200\n").arg(avg60), 100, 1000);
            QVERIFY(!metric.someAvailable);
            QVERIFY(!metric.fullAvailable);
            QVERIFY(!metric.available);
            QCOMPARE(metric.deltaStallMsPerSec, 0.0);
        }
        const auto zero = PsiMemorySampler::parsePsiContent(
            QStringLiteral("some avg10=0 avg60=0 total=200\nfull avg10=0 avg60=0 total=200\n"), 100, 1000);
        QVERIFY(zero.someAvailable);
        QVERIFY(zero.fullAvailable);
        QCOMPARE(zero.someAvg60, 0.0);
    }

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
        QVERIFY(!memBottleneck.causalChain.contains(QStringLiteral("firefox-bin")));
        QCOMPARE(memBottleneck.topMemoryProcess.name, QStringLiteral("firefox-bin"));
        QCOMPARE(memBottleneck.topMemoryProcess.rssMegabytes, 4096.0);

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

        // 4. Power-saver profile active during heavy CPU load
        cpu.deltaThrottleEvents = 0;
        cpu.packageTempCelsius = 68.0;
        cpu.thermalHeadroomCelsius = 32.0;
        cpu.totalLoadPercent = 72.0;
        gpu.busyPercent = 15.0;
        gpu.tempCelsius = 52.0;
        power.onAcPower = false;
        power.activePowerProfile = QStringLiteral("power-saver");
        const CausalVerdict powerSaverMismatch = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(powerSaverMismatch.severity, VerdictSeverity::Elevated);
        QCOMPARE(powerSaverMismatch.primaryBottleneckDomain, QStringLiteral("Power & Cooling"));
        QVERIFY(powerSaverMismatch.headline.contains(QStringLiteral("power-saver")));

        // 4b. Power-saver throttling on AC power under severe CPU load
        power.onAcPower = true;
        cpu.totalLoadPercent = 86.0;
        const CausalVerdict powerThrottling = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(powerThrottling.severity, VerdictSeverity::Bottleneck);
        QCOMPARE(powerThrottling.ruleId, QStringLiteral("power.profile-throttling"));
        QVERIFY(powerThrottling.recommendation.contains(QStringLiteral("header profile toggle")));

        // 5. PL1 Power Limit Capping
        power.activePowerProfile = QStringLiteral("balanced");
        cpu.totalLoadPercent = 84.0;
        power.packagePowerAvailable = true;
        power.packagePowerEstimated = false;
        power.pl1LimitWatts = 28.0;
        power.packageWatts = 27.5;
        const CausalVerdict pl1Capped = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(pl1Capped.severity, VerdictSeverity::Elevated);
        QCOMPARE(pl1Capped.primaryBottleneckDomain, QStringLiteral("Power & Cooling"));
        QVERIFY(pl1Capped.headline.contains(QStringLiteral("PL1")));

        // 6. High battery discharge rate
        cpu.totalLoadPercent = 30.0;
        power.pl1LimitWatts = 0.0;
        power.batteryStateAvailable = true;
        power.batteryPresent = true;
        power.onAcPower = false;
        power.batteryPowerAvailable = true;
        power.batteryLevelAvailable = true;
        power.batteryPercent = 42.0;
        power.batteryWatts = 31.5;
        const CausalVerdict batteryDrain = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(batteryDrain.severity, VerdictSeverity::Elevated);
        QCOMPARE(batteryDrain.primaryBottleneckDomain, QStringLiteral("Power & Cooling"));
        QVERIFY(batteryDrain.headline.contains(QStringLiteral("battery")));

        // 7. Single-Core Saturation verdict
        power.onAcPower = true;
        power.batteryWatts = 0.0;
        cpu.totalLoadPercent = 25.0;
        cpu.cores.clear();
        for (int c = 0; c < 4; ++c) {
            CoreTelemetry ct;
            ct.coreId = c;
            ct.loadAvailable = true;
            ct.loadPercent = (c == 2) ? 99.0 : 1.0;
            cpu.cores.append(ct);
        }
        psi.cpuPsi.someAvailable = true;
        psi.cpuPsi.someAvg10 = 5.0;
        const CausalVerdict singleCoreVerdict = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(singleCoreVerdict.ruleId, QStringLiteral("cpu.single-core-saturation"));
        QCOMPARE(singleCoreVerdict.severity, VerdictSeverity::Elevated);
        QVERIFY(singleCoreVerdict.evidence.first().contains(QStringLiteral("Core 2 is saturated")));

        // 8. NVMe Hardware Critical Warning (CriticalThrottle / Severity 3)
        cpu.cores.clear();
        psi.cpuPsi.someAvg10 = 0.0;
        NvmeControllerTelemetry nvmeCtrl;
        nvmeCtrl.id = QStringLiteral("nvme0");
        nvmeCtrl.model = QStringLiteral("Samsung SSD 980 PRO 1TB");
        nvmeCtrl.warningAvailable = true;
        nvmeCtrl.criticalWarning = 0x05; // bit 0 (spare) + bit 2 (reliability)
        nvmeCtrl.compositeTempAvailable = true;
        nvmeCtrl.compositeTempCelsius = 45.0;
        psi.nvmeControllers.append(nvmeCtrl);

        const CausalVerdict nvmeCritical = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(nvmeCritical.ruleId, QStringLiteral("storage.nvme-critical-warning"));
        QCOMPARE(nvmeCritical.severity, VerdictSeverity::CriticalThrottle);
        QCOMPARE(nvmeCritical.primaryBottleneckDomain, QStringLiteral("Storage"));
        QVERIFY(nvmeCritical.headline.contains(QStringLiteral("NVMe storage hardware critical warning")));
        QVERIFY(nvmeCritical.causalChain.contains(QStringLiteral("available spare below threshold")));

        // 9. NVMe Thermal Threshold Exceeded (Elevated / Severity 1, or Bottleneck with PSI)
        psi.nvmeControllers.first().criticalWarning = 0x02; // bit 1 only (temperature threshold exceeded)
        psi.nvmeControllers.first().compositeTempCelsius = 82.0;
        const CausalVerdict nvmeThermal = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(nvmeThermal.ruleId, QStringLiteral("storage.nvme-thermal-threshold"));
        QCOMPARE(nvmeThermal.severity, VerdictSeverity::Elevated);
        QCOMPARE(nvmeThermal.primaryBottleneckDomain, QStringLiteral("Storage"));
        QVERIFY(nvmeThermal.headline.contains(QStringLiteral("thermal threshold exceeded")));

        // 10. Wi-Fi Link Degradation verdict (Elevated / Severity 1)
        psi.nvmeControllers.clear();
        psi.wifiQualityAvailable = true;
        psi.primaryWifiSignalPercent = 18;
        psi.primaryWifiSignalDbm = -82;
        psi.primaryWifiSsid = QStringLiteral("Home-WiFi");
        psi.netThroughputAvailable = true;
        psi.netRxMibPerSec = 4.5;
        psi.netTxMibPerSec = 0.8;
        const CausalVerdict wifiDegraded = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(wifiDegraded.ruleId, QStringLiteral("network.wifi-link-degradation"));
        QCOMPARE(wifiDegraded.severity, VerdictSeverity::Elevated);
        QCOMPARE(wifiDegraded.primaryBottleneckDomain, QStringLiteral("Storage & Network"));
        QVERIFY(wifiDegraded.causalChain.contains(QStringLiteral("Home-WiFi")));
        QVERIFY(wifiDegraded.causalChain.contains(QStringLiteral("18%")));

        // 11. ZRAM Memory Exhaustion verdict (Bottleneck / Severity 2)
        psi.wifiQualityAvailable = false;
        psi.zramActive = true;
        psi.zramDiskSizeBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
        psi.zramOrigBytes = 7ULL * 1024ULL * 1024ULL * 1024ULL;
        psi.zramCompressedBytes = 3ULL * 1024ULL * 1024ULL * 1024ULL;
        psi.zramMemUsedTotalBytes = 3ULL * 1024ULL * 1024ULL * 1024ULL;
        psi.zramCompressionRatio = 2.33;
        psi.memoryPsi.someAvailable = true;
        psi.memoryPsi.someAvg10 = 5.5;
        const CausalVerdict zramExhausted = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, power, domains, culprit);
        QCOMPARE(zramExhausted.ruleId, QStringLiteral("memory.zram-exhaustion"));
        QCOMPARE(zramExhausted.severity, VerdictSeverity::Bottleneck);
        QCOMPARE(zramExhausted.primaryBottleneckDomain, QStringLiteral("Memory"));
        QVERIFY(zramExhausted.headline.contains(QStringLiteral("ZRAM compressed swap")));
        QVERIFY(zramExhausted.causalChain.contains(QStringLiteral("88% filled")));
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

    void testMultiFanAndNvmeControllerDiscovery()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString sysRoot = tempDir.filePath(QStringLiteral("sys"));
        const QString procRoot = tempDir.filePath(QStringLiteral("proc"));

        // Setup hwmon with two fans
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/name"), QStringLiteral("nct6775\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/fan1_label"), QStringLiteral("CPU Fan\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/fan1_input"), QStringLiteral("1450\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/fan2_label"), QStringLiteral("Chassis Fan 1\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/fan2_input"), QStringLiteral("980\n"));

        PowerFlowSampler powerSampler({sysRoot, procRoot});
        const PowerFlowSnapshot powerSnap = powerSampler.sample(1000, 25.0, 2000, 4000);
        QVERIFY(powerSnap.fanAvailable);
        QCOMPARE(powerSnap.primaryFanRpm, 1450);
        QCOMPARE(powerSnap.fans.size(), 2);
        QCOMPARE(powerSnap.fans.at(0).label, QStringLiteral("CPU Fan"));
        QCOMPARE(powerSnap.fans.at(0).rpm, 1450);
        QCOMPARE(powerSnap.fans.at(1).label, QStringLiteral("Chassis Fan 1"));
        QCOMPARE(powerSnap.fans.at(1).rpm, 980);

        // Setup nvme controller in sysfs
        writeTextFile(sysRoot + QStringLiteral("/class/nvme/nvme0/model"), QStringLiteral("Samsung 980 PRO 1TB\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/nvme/nvme0/state"), QStringLiteral("live\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/nvme/nvme0/critical_warning"), QStringLiteral("0\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/nvme/nvme0/hwmon2/temp1_input"), QStringLiteral("41000\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/nvme/nvme0/hwmon2/temp1_alarm"), QStringLiteral("0\n"));

        writeTextFile(procRoot + QStringLiteral("/stat"), QStringLiteral("cpu  100 0 100 800 0 0 0 0 0 0\n"));
        writeTextFile(procRoot + QStringLiteral("/meminfo"), QStringLiteral("MemTotal:        16384000 kB\nMemAvailable:     8192000 kB\n"));

        PsiMemorySampler psiSampler({sysRoot, procRoot});
        const PsiAndMemorySnapshot psiSnap = psiSampler.sample(1000);
        QCOMPARE(psiSnap.nvmeControllers.size(), 1);
        QCOMPARE(psiSnap.nvmeControllers.first().id, QStringLiteral("nvme0"));
        QCOMPARE(psiSnap.nvmeControllers.first().model, QStringLiteral("Samsung 980 PRO 1TB"));
        QCOMPARE(psiSnap.nvmeControllers.first().state, QStringLiteral("live"));
        QVERIFY(psiSnap.nvmeControllers.first().warningAvailable);
        QCOMPARE(psiSnap.nvmeControllers.first().criticalWarning, 0);
        QVERIFY(!psiSnap.nvmeControllers.first().criticalWarningAlarm);
        QVERIFY(psiSnap.nvmeControllers.first().compositeTempAvailable);
        QCOMPARE(psiSnap.nvmeControllers.first().compositeTempCelsius, 41.0);
    }

    void testKernelLogReaderParsingAndRedaction()
    {
        const QString mockJournal = QStringLiteral(
            "2026-10-24T21:38:40+02:00 myhost kernel: thermal thermal_zone0: critical temperature reached (100 C), throttling\n"
            "2026-10-24T21:38:42+02:00 myhost kernel: Bluetooth: hci0: unexpected packet received\n"
            "2026-10-24T21:38:45+02:00 myhost kernel: amdgpu 0000:03:00.0: [drm:amdgpu_job_timedout] *ERROR* ring gfx_0.0.0 timeout for /home/johndoe/projects/game\n"
            "2026-10-24T21:38:50+02:00 myhost kernel: oom-killer: gfp_mask=0x100cca(GFP_HIGHUSER_MOVABLE), order=0, oom_score_adj=0 from 192.168.1.100 with dev 00:1a:2b:3c:4d:5e\n"
        );

        const QList<KernelLogEntry> entries = KernelLogReader::parseJournalLines(mockJournal);
        QCOMPARE(entries.size(), 3); // Bluetooth should be filtered out

        // 1. Thermal
        QCOMPARE(entries.at(0).subsystem, QStringLiteral("Thermal"));
        QCOMPARE(entries.at(0).severity, QStringLiteral("error"));
        QVERIFY(entries.at(0).message.contains(QStringLiteral("critical temperature reached")));

        // 2. GPU DRM error with path redaction
        QCOMPARE(entries.at(1).subsystem, QStringLiteral("Graphics"));
        QCOMPARE(entries.at(1).severity, QStringLiteral("error"));
        QVERIFY(entries.at(1).message.contains(QStringLiteral("/home/[user]/projects/game")));
        QVERIFY(!entries.at(1).message.contains(QStringLiteral("johndoe")));

        // 3. OOM Killer with IP and MAC redaction
        QCOMPARE(entries.at(2).subsystem, QStringLiteral("Memory/OOM"));
        QCOMPARE(entries.at(2).severity, QStringLiteral("error"));
        QVERIFY(entries.at(2).message.contains(QStringLiteral("[ip-redacted]")));
        QVERIFY(entries.at(2).message.contains(QStringLiteral("[mac-redacted]")));
        QVERIFY(!entries.at(2).message.contains(QStringLiteral("192.168.1.100")));
        QVERIFY(!entries.at(2).message.contains(QStringLiteral("00:1a:2b:3c:4d:5e")));
    }

    void testFlightRecorderCompareIncidents()
    {
        FlightRecorder recorder;
        FlightSample s1;
        s1.sampleId = 1;
        s1.timestampMs = 1000000;
        s1.cpuTempCelsius = 60.0;
        s1.cpuLoadPercent = 20.0;
        s1.responsivenessScore = 95.0;
        recorder.appendSample(s1);

        const QString marker1 = recorder.markProblemNow(QStringLiteral("First problem"));

        FlightSample s2;
        s2.sampleId = 2;
        s2.timestampMs = 1100000; // 100 seconds later, well outside the 30s context window
        s2.cpuTempCelsius = 85.0;
        s2.cpuLoadPercent = 88.0;
        s2.responsivenessScore = 65.0;
        s2.culpritName = QStringLiteral("compiler");
        s2.culpritCpuPercent = 90.0;
        recorder.appendSample(s2);

        const QString marker2 = recorder.markProblemNow(QStringLiteral("Second problem"));

        const QVariantMap comp = recorder.compareIncidents(marker1, marker2);
        QVERIFY(!comp.isEmpty());
        QVERIFY(comp.contains(QStringLiteral("incidentA")));
        QVERIFY(comp.contains(QStringLiteral("incidentB")));
        QVERIFY(comp.contains(QStringLiteral("diff")));
        const QVariantMap diff = comp.value(QStringLiteral("diff")).toMap();
        QCOMPARE(diff.value(QStringLiteral("deltaPeakTemp")).toDouble(), 25.0);
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

        const QString md = rec2.exportSummaryMarkdown(true);
        QVERIFY(md.contains(QStringLiteral("rustc")));
        QVERIFY(md.contains(QStringLiteral("Peak Thermal Spike")));

        const QString json = rec2.exportSamplesJson(true);
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
        QCOMPARE(exported.value(QStringLiteral("schemaVersion")).toInt(), 2);
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
        QVERIFY(md.contains(QStringLiteral("**NVMe Storage Temperature**: 44.0°C")));
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

    void testCpuHwmonPowerAndDiskStatsThroughput()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString sysRoot = tempDir.filePath(QStringLiteral("sys"));
        const QString procRoot = tempDir.filePath(QStringLiteral("proc"));

        // 1. CPU hwmon package power (zenpower) when intel-rapl is absent
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/name"), QStringLiteral("zenpower\n"));
        writeTextFile(sysRoot + QStringLiteral("/class/hwmon/hwmon0/power1_average"), QStringLiteral("42500000\n"));
        PowerFlowSampler powerSampler({sysRoot, procRoot});
        const PowerFlowSnapshot powerSnap = powerSampler.sample(1000, 50.0, 3000, 4500);
        QVERIFY(powerSnap.hwmonPowerAvailable);
        QVERIFY(powerSnap.packagePowerAvailable);
        QVERIFY(!powerSnap.packagePowerEstimated);
        QCOMPARE(powerSnap.packageWatts, 42.5);

        // 2. Intel xe GPU frequency path
        const QString xeCard = sysRoot + QStringLiteral("/class/drm/card0");
        writeTextFile(xeCard + QStringLiteral("/device/tile0/gt0/freq0/act_freq"), QStringLiteral("1450\n"));
        writeTextFile(xeCard + QStringLiteral("/device/tile0/gt0/freq0/max_freq"), QStringLiteral("2200\n"));
        CpuSocSampler cpuSampler({sysRoot, procRoot});
        const GpuSnapshot xeGpu = cpuSampler.sampleGpu({});
        QVERIFY(xeGpu.available);
        QVERIFY(xeGpu.frequencyAvailable);
        QCOMPARE(xeGpu.currentFreqMhz, 1450);
        QCOMPARE(xeGpu.boostFreqMhz, 2200);

        // 3. /proc/diskstats throughput (ignoring partitions like nvme0n1p1 and zram0)
        writeTextFile(procRoot + QStringLiteral("/pressure/io"), QStringLiteral("some avg10=12.00 avg60=5.00 total=1000\nfull avg10=9.50 avg60=4.00 total=800\n"));
        writeTextFile(procRoot + QStringLiteral("/diskstats"),
                      QStringLiteral(" 259 0 nvme0n1 100 0 2048 10 200 0 4096 20 0 10 30\n"
                                     " 259 1 nvme0n1p1 100 0 2048 10 200 0 4096 20 0 10 30\n"
                                     " 252 0 zram0 50 0 1024 5 50 0 1024 5 0 5 10\n"));
        PsiMemorySampler psiSampler({sysRoot, procRoot});
        const PsiAndMemorySnapshot firstPsi = psiSampler.sample(1000);
        QVERIFY(!firstPsi.diskThroughputAvailable);

        // Advance nvme0n1 by +4096 read sectors (2 MiB) and +8192 write sectors (4 MiB) over 1000 ms
        writeTextFile(procRoot + QStringLiteral("/diskstats"),
                      QStringLiteral(" 259 0 nvme0n1 150 0 6144 15 250 0 12288 25 0 15 40\n"
                                     " 259 1 nvme0n1p1 150 0 6144 15 250 0 12288 25 0 15 40\n"));
        const PsiAndMemorySnapshot secondPsi = psiSampler.sample(1000);
        QVERIFY(secondPsi.diskThroughputAvailable);
        QCOMPARE(secondPsi.diskReadMibPerSec, 2.0);
        QCOMPARE(secondPsi.diskWriteMibPerSec, 4.0);

        CpuSocSnapshot cpu;
        cpu.loadAvailable = true;
        cpu.loadWarmedUp = true;
        cpu.temperatureAvailable = true;
        const CausalVerdict storageVerdict = CausalDiagnosisEngine::evaluate(cpu, {}, secondPsi, powerSnap, {}, {});
        QCOMPARE(storageVerdict.severity, VerdictSeverity::Bottleneck);
        QVERIFY(storageVerdict.causalChain.contains(QStringLiteral("2.0 MiB/s read, 4.0 MiB/s write")));
    }

    void testNetworkThroughputAndProcessRadarDiskWait()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString sysRoot = tempDir.filePath(QStringLiteral("sys"));
        const QString procRoot = tempDir.filePath(QStringLiteral("proc"));

        writeTextFile(procRoot + QStringLiteral("/net/dev"),
                      QStringLiteral("Inter-|   Receive                                                |  Transmit\n"
                                     " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
                                     "    lo: 104857600 100 0 0 0 0 0 0 104857600 100 0 0 0 0 0 0\n"
                                     "docker0: 52428800 50 0 0 0 0 0 0 52428800 50 0 0 0 0 0 0\n"
                                     " wlan0: 1048576 10 0 0 0 0 0 0 2097152 20 0 0 0 0 0 0\n"));

        writeTextFile(sysRoot + QStringLiteral("/class/net/wlan0/device/uevent"), QStringLiteral("PHYSICAL=1\n"));
        PsiMemorySampler psiSampler({sysRoot, procRoot});
        const PsiAndMemorySnapshot firstPsi = psiSampler.sample(1000);
        QVERIFY(!firstPsi.netThroughputAvailable);

        // Advance wlan0 by +3 MiB RX and +1.5 MiB TX; advance lo and docker0 heavily to verify they are ignored
        writeTextFile(procRoot + QStringLiteral("/net/dev"),
                      QStringLiteral("Inter-|   Receive                                                |  Transmit\n"
                                     " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
                                     "    lo: 999999999 200 0 0 0 0 0 0 999999999 200 0 0 0 0 0 0\n"
                                     "docker0: 888888888 150 0 0 0 0 0 0 888888888 150 0 0 0 0 0 0\n"
                                     " wlan0: 4194304 40 0 0 0 0 0 0 3670016 35 0 0 0 0 0 0\n"));
        const PsiAndMemorySnapshot secondPsi = psiSampler.sample(1000);
        QVERIFY(secondPsi.netThroughputAvailable);
        QCOMPARE(secondPsi.netRxMibPerSec, 3.0);
        QCOMPARE(secondPsi.netTxMibPerSec, 1.5);

        // Process Radar D-state (uninterruptible disk sleep) detection
        writeTextFile(procRoot + QStringLiteral("/stat"),
                      QStringLiteral("cpu  1000 0 500 8500 0 0 0 0 0 0\n"
                                     "cpu0 1000 0 500 8500 0 0 0 0 0 0\n"));
        writeTextFile(procRoot + QStringLiteral("/101/stat"),
                      QStringLiteral("101 (rsync-backup) D 1 101 101 0 -1 4194304 0 0 0 0 10 20 0 0 20 0 1 0 100 104857600 2500 18446744073709551615 0 0 0 0 0 0 0 0 0 0 0 0 17 0 0 0 0 0 0\n"));
        writeTextFile(procRoot + QStringLiteral("/101/status"),
                      QStringLiteral("Name:\trsync-backup\nVmRSS:\t20480 kB\n"));
        writeTextFile(procRoot + QStringLiteral("/202/stat"),
                      QStringLiteral("202 (kwin_wayland) S 1 202 202 0 -1 4194304 0 0 0 0 50 50 0 0 20 0 1 0 100 209715200 5000 18446744073709551615 0 0 0 0 0 0 0 0 0 0 0 0 17 0 0 0 0 0 0\n"));
        writeTextFile(procRoot + QStringLiteral("/202/status"),
                      QStringLiteral("Name:\tkwin_wayland\nVmRSS:\t65536 kB\n"));

        CpuSocSampler cpuSampler({sysRoot, procRoot});
        cpuSampler.sampleCpu({}, 1000);
        const ProcessRadarSnapshot radar = cpuSampler.sampleProcessRadar(5);
        QCOMPARE(radar.diskWaitProcessCount, 1);
        QCOMPARE(radar.topDiskWaitProcess.pid, 101);
        QVERIFY(radar.topDiskWaitProcess.inDiskWait);
        QCOMPARE(radar.topDiskWaitProcess.state, QStringLiteral("D"));
        QVERIFY(!radar.topProcesses.isEmpty());
        QCOMPARE(radar.topProcesses.first().pid, 101);
    }

    void testMultiDomainContributingSignalsAndStabilizerOscillation()
    {
        CpuSocSnapshot cpu;
        cpu.loadAvailable = true;
        cpu.loadWarmedUp = true;
        cpu.temperatureAvailable = true;
        cpu.packageTempCelsius = 85.0;
        cpu.totalLoadPercent = 95.0;

        PsiAndMemorySnapshot psi;
        psi.responsivenessAvailable = true;
        psi.cpuPsi.someAvailable = true;
        psi.cpuPsi.someAvg10 = 22.0;
        psi.memoryPsi.someAvailable = true;
        psi.memoryPsi.someAvg10 = 18.0;
        psi.memoryPsi.fullAvailable = true;
        psi.memoryPsi.fullAvg10 = 8.0;
        psi.ioPsi.someAvailable = true;
        psi.ioPsi.fullAvailable = true;
        psi.ioPsi.fullAvg10 = 15.0;
        psi.ramAvailable = true;
        psi.ramUsedPercent = 94.0;

        const CausalVerdict multiVerdict = CausalDiagnosisEngine::evaluate(cpu, {}, psi, {}, {}, {});
        QCOMPARE(multiVerdict.severity, VerdictSeverity::Bottleneck);
        QCOMPARE(multiVerdict.contributingSignals.size(), 2);

        // GPU thermal alert when gpu_busy_percent is unavailable (e.g. Intel i915/xe)
        CpuSocSnapshot idleCpu;
        idleCpu.loadAvailable = true;
        idleCpu.loadWarmedUp = true;
        idleCpu.temperatureAvailable = true;
        idleCpu.packageTempCelsius = 55.0;
        GpuSnapshot hotIntelGpu;
        hotIntelGpu.available = true;
        hotIntelGpu.busyAvailable = false;
        hotIntelGpu.temperatureAvailable = true;
        hotIntelGpu.tempCelsius = 90.0;
        PsiAndMemorySnapshot calmPsi;
        calmPsi.responsivenessAvailable = true;
        calmPsi.cpuPsi.someAvailable = true;
        calmPsi.memoryPsi.someAvailable = true;
        calmPsi.ioPsi.someAvailable = true;

        const CausalVerdict gpuThermalVerdict = CausalDiagnosisEngine::evaluate(idleCpu, hotIntelGpu, calmPsi, {}, {}, {});
        QCOMPARE(gpuThermalVerdict.severity, VerdictSeverity::Elevated);
        QCOMPARE(gpuThermalVerdict.primaryBottleneckDomain, QStringLiteral("Graphics"));

        // VerdictStabilizer should not starve when oscillating between Elevated and Bottleneck
        VerdictStabilizer stabilizer;
        CausalVerdict nominal;
        nominal.severity = VerdictSeverity::Nominal;
        nominal.headline = QStringLiteral("Nominal");
        stabilizer.update(nominal, 0);
        QCOMPARE(stabilizer.update(nominal, 5000).severity, VerdictSeverity::Nominal);

        CausalVerdict elevated;
        elevated.severity = VerdictSeverity::Elevated;
        elevated.headline = QStringLiteral("Elevated");
        CausalVerdict bottleneck;
        bottleneck.severity = VerdictSeverity::Bottleneck;
        bottleneck.headline = QStringLiteral("Bottleneck");

        QCOMPARE(stabilizer.update(elevated, 6000).severity, VerdictSeverity::Nominal);
        QCOMPARE(stabilizer.update(bottleneck, 7500).severity, VerdictSeverity::Nominal);
        QCOMPARE(stabilizer.update(elevated, 9100).severity, VerdictSeverity::Elevated);

        // FlightRecorder immediate pruning when setRetentionMs shrinks window
        FlightRecorder recorder(3600);
        recorder.setRetentionMs(10000);
        for (int i = 1; i <= 10; ++i) {
            FlightSample s;
            s.monotonicMs = i * 1000;
            s.timestampMs = i * 1000;
            recorder.appendSample(s);
        }
        QCOMPARE(recorder.count(), 10);
        recorder.setRetentionMs(3000);
        QCOMPARE(recorder.count(), 4);
        QCOMPARE(recorder.sampleAt(0).monotonicMs, qint64(7000));
    }

    void testIncidentEpisodesRespectRecordingGaps()
    {
        FlightRecorder recorder;
        for (qint64 time : {1000, 2000, 63000, 64000}) {
            FlightSample sample;
            sample.timestampMs = 1700000000000LL + time;
            sample.monotonicMs = time;
            sample.severity = VerdictSeverity::Elevated;
            sample.verdictHeadline = QStringLiteral("Pressure observed");
            sample.elapsedSincePreviousMs = time == 63000 ? 61000 : 1000;
            sample.metricQuality.insert(QStringLiteral("gapBefore"), time == 63000);
            recorder.appendSample(sample);
        }
        const auto episodes = recorder.incidentEpisodes();
        QCOMPARE(episodes.size(), 2);
        QCOMPARE(episodes[0].toMap().value(QStringLiteral("durationSeconds")).toInt(), 1);
        QCOMPARE(episodes[1].toMap().value(QStringLiteral("durationSeconds")).toInt(), 1);
        QVERIFY(episodes[0].toMap().value(QStringLiteral("sampleId")) != episodes[1].toMap().value(QStringLiteral("sampleId")));
    }

    void testHighActivityWithoutRiskIsNeutral()
    {
        CpuSocSnapshot cpu;
        cpu.loadAvailable = true;
        cpu.loadWarmedUp = true;
        cpu.temperatureAvailable = true;
        cpu.thermalLimitAvailable = true;
        cpu.totalLoadPercent = 98;
        cpu.packageTempCelsius = 45;
        cpu.thermalHeadroomCelsius = 55;
        cpu.thermalVelocityCPerSec = 1.5;
        PsiAndMemorySnapshot psi;
        psi.responsivenessAvailable = true;
        psi.cpuPsi.someAvailable = true;
        GpuSnapshot gpu;
        gpu.busyAvailable = true;
        gpu.temperatureAvailable = true;
        gpu.busyPercent = 99;
        gpu.tempCelsius = 45;
        auto verdict = CausalDiagnosisEngine::evaluate(cpu, gpu, psi, {}, {}, {});
        QCOMPARE(verdict.severity, VerdictSeverity::Nominal);
        VerdictStabilizer stabilizer;
        stabilizer.update(verdict, 1000);
        QCOMPARE(stabilizer.update(verdict, 6000).severity, VerdictSeverity::Nominal);
        cpu.totalLoadPercent = 10;
        QCOMPARE(CausalDiagnosisEngine::evaluate(cpu, gpu, psi, {}, {}, {}).severity, VerdictSeverity::Nominal);
        psi.cpuPsi.someAvg10 = 15;
        QCOMPARE(CausalDiagnosisEngine::evaluate(cpu, gpu, psi, {}, {}, {}).severity, VerdictSeverity::Elevated);
    }

    void testRetentionMatrixAndCapacity()
    {
        for (const int interval : {250, 1000, 10000}) {
            for (const int seconds : {60, 300, 1800}) {
                FlightRecorder recorder;
                recorder.setRetentionMs(seconds * 1000LL);
                const qint64 total = (seconds + 20) * 1000LL;
                for (qint64 time = 1000; time <= total + 1000; time += interval) {
                    FlightSample sample;
                    sample.monotonicMs = time;
                    recorder.appendSample(sample);
                }
                const auto &samples = recorder.samples();
                QCOMPARE(samples.last().monotonicMs - samples.first().monotonicMs, qint64(seconds * 1000));
                QVERIFY(samples.size() <= recorder.capacity());
            }
        }
        FlightRecorder bounded;
        for (int i = 0; i < 50; ++i) bounded.appendSample({});
        bounded.setCapacity(10);
        QCOMPARE(bounded.count(), 10);
        QCOMPARE(bounded.sampleAt(0).sampleId, quint64(41));
    }

    void testDeviceBaselinesHotplugResetAndVirtualSelection()
    {
        QTemporaryDir temp;
        const QString sys = temp.filePath(QStringLiteral("sys"));
        const QString proc = temp.filePath(QStringLiteral("proc"));
        writeTextFile(sys + QStringLiteral("/class/net/eth0/device/uevent"), QStringLiteral("DRIVER=test"));
        writeTextFile(sys + QStringLiteral("/class/net/eth0/ifindex"), QStringLiteral("2"));
        writeTextFile(sys + QStringLiteral("/class/net/tun0/ifindex"), QStringLiteral("3"));
        const auto netLine = [](const QString &name, quint64 value) {
            return QStringLiteral("%1: %2 0 0 0 0 0 0 0 %2 0 0 0 0 0 0 0\n").arg(name).arg(value);
        };
        const auto diskLine = [](int minor, const QString &name, quint64 value) {
            return QStringLiteral("8 %1 %2 0 0 %3 0 0 0 %3 0 0\n").arg(minor).arg(name).arg(value);
        };
        writeTextFile(proc + QStringLiteral("/net/dev"), netLine(QStringLiteral("eth0"), 1048576) + netLine(QStringLiteral("tun0"), 1048576));
        writeTextFile(proc + QStringLiteral("/diskstats"), diskLine(0, QStringLiteral("sda"), 2048));
        PsiMemorySampler sampler({sys, proc});
        sampler.sample(1000);
        writeTextFile(proc + QStringLiteral("/net/dev"), netLine(QStringLiteral("eth0"), 2097152) + netLine(QStringLiteral("tun0"), 2097152) + netLine(QStringLiteral("eth1"), 999999999));
        writeTextFile(proc + QStringLiteral("/diskstats"), diskLine(0, QStringLiteral("sda"), 4096) + diskLine(16, QStringLiteral("sdb"), 999999999));
        const auto hotplug = sampler.sample(1000);
        QCOMPARE(hotplug.netRxMibPerSec, 1.0);
        QCOMPARE(hotplug.diskReadMibPerSec, 1.0);
        QCOMPARE(hotplug.networkInterfaces.size(), 3);
        QVERIFY(hotplug.networkInterfaces.first().isPhysical);
        sampler.setIncludedNetworkInterfaces({QStringLiteral("tun0")});
        QVERIFY(!sampler.sample(1000).netThroughputAvailable);
        writeTextFile(proc + QStringLiteral("/net/dev"), netLine(QStringLiteral("eth0"), 3145728) + netLine(QStringLiteral("tun0"), 4194304));
        QCOMPARE(sampler.sample(1000).netRxMibPerSec, 2.0);
        writeTextFile(proc + QStringLiteral("/net/dev"), netLine(QStringLiteral("tun0"), 10));
        writeTextFile(proc + QStringLiteral("/diskstats"), diskLine(0, QStringLiteral("sda"), 10));
        const auto reset = sampler.sample(1000);
        QVERIFY(!reset.netThroughputAvailable);
        QVERIFY(!reset.diskThroughputAvailable);
        writeTextFile(proc + QStringLiteral("/net/dev"), netLine(QStringLiteral("tun0"), 1048586));
        QCOMPARE(sampler.sample(1000).netRxMibPerSec, 1.0);
        writeTextFile(sys + QStringLiteral("/class/net/tun0/ifindex"), QStringLiteral("4"));
        QVERIFY(!sampler.sample(1000).netThroughputAvailable);
    }

    void testProcessIdentityAndObservedTimestamp()
    {
        QTemporaryDir temp;
        const QString proc = temp.filePath(QStringLiteral("proc"));
        const auto process = [](quint64 ticks, quint64 start) {
            QStringList fields;
            for (int i = 0; i <= 21; ++i) fields.append(QStringLiteral("0"));
            fields[0] = QStringLiteral("R"); fields[11] = QString::number(ticks);
            fields[19] = QString::number(start); fields[21] = QStringLiteral("4096");
            return QStringLiteral("123 (worker) ") + fields.join(QLatin1Char(' '));
        };
        const auto stat = [&proc](int ticks) {
            writeTextFile(proc + QStringLiteral("/stat"), QStringLiteral("cpu %1 0 0 100 0 0 0 0\ncpu0 %1 0 0 100 0 0 0 0\n").arg(ticks));
        };
        CpuSocSampler sampler({temp.filePath(QStringLiteral("sys")), proc});
        stat(100); writeTextFile(proc + QStringLiteral("/123/stat"), process(100, 1));
        sampler.sampleCpu({}); sampler.sampleProcessRadar();
        stat(200); writeTextFile(proc + QStringLiteral("/123/stat"), process(150, 1));
        sampler.sampleCpu({}); const auto original = sampler.sampleProcessRadar();
        QCOMPARE(original.topCpuProcess.cpuPercent, 50.0);
        QVERIFY(original.observedAtMs > 0);
        QCOMPARE(original.topCpuProcess.observedAtMs, original.observedAtMs);
        stat(300); writeTextFile(proc + QStringLiteral("/123/stat"), process(200, 2));
        sampler.sampleCpu({}); const auto reused = sampler.sampleProcessRadar();
        QCOMPARE(reused.topCpuProcess.pid, 0);
        QCOMPARE(reused.topMemoryProcess.startTimeTicks, quint64(2));
        QCOMPARE(reused.topMemoryProcess.cpuPercent, 0.0);
    }

    void testRuleIdentityStabilizationAndEvidence()
    {
        VerdictStabilizer stabilizer;
        CausalVerdict first;
        first.ruleId = QStringLiteral("cpu.elevated"); first.severity = VerdictSeverity::Elevated;
        first.evidence = {QStringLiteral("CPU load 90%")};
        stabilizer.update(first, 0);
        QCOMPARE(stabilizer.update(first, 3000).ruleId, first.ruleId);
        CausalVerdict second = first;
        second.ruleId = QStringLiteral("gpu.temperature");
        QCOMPARE(stabilizer.update(second, 3100).ruleId, first.ruleId);
        QCOMPARE(stabilizer.update(first, 4000).ruleId, first.ruleId);
        QCOMPARE(stabilizer.update(second, 5000).ruleId, first.ruleId);
        const auto confirmed = stabilizer.update(second, 8000);
        QCOMPARE(confirmed.ruleId, second.ruleId);
        QCOMPARE(confirmed.observedSinceMs, qint64(5000));
        QCOMPARE(confirmed.durationMs, qint64(3000));
        QCOMPARE(confirmed.evidence, first.evidence);
        second.severity = VerdictSeverity::CriticalThrottle;
        second.ruleId = QStringLiteral("cpu.thermal-throttle");
        QCOMPARE(stabilizer.update(second, 8001).ruleId, second.ruleId);
        CpuSocSnapshot cpu;
        cpu.throttleCountersAvailable = true; cpu.deltaThrottleEvents = 2;
        const auto verdict = CausalDiagnosisEngine::evaluate(cpu, {}, {}, {}, {}, {});
        QCOMPARE(verdict.ruleId, QStringLiteral("cpu.thermal-throttle"));
        QVERIFY(!verdict.evidence.isEmpty());
        PsiAndMemorySnapshot pressure;
        pressure.memoryPsi.someAvailable = true; pressure.memoryPsi.someAvg10 = 10.0;
        CulpritProcess privateProcess; privateProcess.name = QStringLiteral("private-memory-worker");
        const auto memory = CausalDiagnosisEngine::evaluate({}, {}, pressure, {}, {}, {}, privateProcess);
        QVERIFY(!memory.causalChain.contains(privateProcess.name));
        QVERIFY(!memory.evidence.join(QLatin1Char(' ')).contains(privateProcess.name));
    }

    void testExportV2PowerDomainsAndProcessRedaction()
    {
        FlightRecorder recorder;
        FlightSample sample;
        sample.timestampMs = 1000; sample.cpuTempCelsius = 90.0; sample.responsivenessScore = 70;
        sample.packageWatts = 12.0; sample.batteryWatts = 25.0; sample.systemWatts = 25.0;
        sample.culpritName = QStringLiteral("private-worker"); sample.culpritPid = 1234;
        sample.culpritStartTimeTicks = 987654321; sample.processObservedAtMs = 876543210;
        sample.verdictRuleId = QStringLiteral("cpu.elevated");
        sample.verdictEvidence = {QStringLiteral("Concurrent private-worker")};
        sample.verdictDurationMs = 3000;
        for (const auto &key : {"cpuTempAvailable", "psiAvailable", "packagePowerAvailable", "batteryPowerAvailable", "batteryDischargeAvailable"})
            sample.metricQuality.insert(QString::fromLatin1(key), true);
        recorder.appendSample(sample);
        const QString json = recorder.exportSamplesJson();
        const QString md = recorder.exportSummaryMarkdown();
        QVERIFY(!json.contains(QStringLiteral("private-worker")));
        QVERIFY(!json.contains(QStringLiteral("1234")));
        QVERIFY(!json.contains(QStringLiteral("987654321")));
        QVERIFY(!json.contains(QStringLiteral("876543210")));
        QVERIFY(!md.contains(QStringLiteral("private-worker")));
        QVERIFY(!md.contains(QStringLiteral("1234")));
        QVERIFY(md.contains(QStringLiteral("CPU package power 12.0 W")));
        QVERIFY(md.contains(QStringLiteral("Battery discharge**: 25.0 W")));
        QVERIFY(!md.contains(QStringLiteral("system power")));
        const auto object = QJsonDocument::fromJson(json.toUtf8()).array().first().toObject();
        QCOMPARE(object.value(QStringLiteral("schemaVersion")).toInt(), 2);
        QCOMPARE(object.value(QStringLiteral("verdictRuleId")).toString(), sample.verdictRuleId);
        QCOMPARE(object.value(QStringLiteral("verdictDurationMs")).toInteger(), qint64(3000));
        QVERIFY(object.contains(QStringLiteral("powerSources")));
        QVERIFY(recorder.exportSamplesJson(true).contains(QStringLiteral("private-worker")));
        QVERIFY(recorder.exportSummaryMarkdown(true).contains(QStringLiteral("private-worker")));
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
