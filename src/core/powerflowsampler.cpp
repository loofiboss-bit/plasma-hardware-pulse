#include "powerflowsampler.h"
#include "sysfsreader.h"

#include <QDir>

#include <algorithm>

namespace LoofiPulse
{

PowerFlowSampler::PowerFlowSampler(SysfsRootConfig config)
    : m_config(std::move(config))
{
}

void PowerFlowSampler::setRootConfig(const SysfsRootConfig &config)
{
    m_config = config;
    m_prevRaplEnergyUj = 0;
    m_hasPrevRapl = false;
}

PowerFlowSnapshot PowerFlowSampler::sample(qint64 elapsedMs, double cpuLoadPercent, int avgFreqMhz, int maxFreqMhz)
{
    PowerFlowSnapshot snap;
    const QDir psyRoot(QDir(m_config.sysRoot).filePath(QStringLiteral("class/power_supply")));
    bool externalKnown = false, externalOnline = false;
    if (psyRoot.exists()) {
        const auto entries = psyRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto &entry : entries) {
            const QDir dev(psyRoot.filePath(entry));
            const QString type = SysfsReader::readTrimmedLine(dev.filePath(QStringLiteral("type"))).toLower();
            if (type != QStringLiteral("battery") && !entry.startsWith(QStringLiteral("BAT"))) {
                bool ok = false;
                const auto online = SysfsReader::readInt64(dev.filePath(QStringLiteral("online")), 0, &ok);
                if (ok && (type == QStringLiteral("mains") || type.startsWith(QStringLiteral("usb")) || entry.startsWith(QStringLiteral("AC")))) {
                    externalKnown = true;
                    externalOnline = externalOnline || online != 0;
                }
                continue;
            }
            BatteryTelemetry battery;
            battery.identity = dev.canonicalPath();
            battery.name = entry;
            bool presentOk = false;
            const auto present = SysfsReader::readInt64(dev.filePath(QStringLiteral("present")), 1, &presentOk);
            battery.present = !presentOk || present != 0;
            if (!battery.present) { snap.batteries.append(battery); continue; }
            battery.status = SysfsReader::readTrimmedLine(dev.filePath(QStringLiteral("status")));
            auto read = [&dev](const QString &file, bool &ok) { return SysfsReader::readScaledDouble(dev.filePath(file), 1000000.0, 0.0, &ok); };
            battery.percent = SysfsReader::readScaledDouble(dev.filePath(QStringLiteral("capacity")), 1.0, 0.0, &battery.levelAvailable);
            battery.levelAvailable = battery.levelAvailable && battery.percent >= 0 && battery.percent <= 100;
            battery.voltageVolts = read(QStringLiteral("voltage_now"), battery.voltageAvailable);
            battery.voltageAvailable = battery.voltageAvailable && battery.voltageVolts > 0;
            battery.watts = read(QStringLiteral("power_now"), battery.powerAvailable);
            battery.powerAvailable = battery.powerAvailable && battery.watts >= 0;
            if (!battery.powerAvailable && battery.voltageAvailable) {
                bool currentOk = false;
                const auto current = read(QStringLiteral("current_now"), currentOk);
                battery.powerAvailable = currentOk && current >= 0;
                if (battery.powerAvailable) battery.watts = current * battery.voltageVolts;
            }
            bool nowOk = false, fullOk = false, designOk = false;
            battery.energyNowWh = read(QStringLiteral("energy_now"), nowOk);
            battery.energyFullWh = read(QStringLiteral("energy_full"), fullOk);
            battery.energyDesignWh = read(QStringLiteral("energy_full_design"), designOk);
            // Charge can only become energy with a measured voltage. Never invent one.
            if (!nowOk && battery.voltageAvailable) {
                battery.energyNowWh = read(QStringLiteral("charge_now"), nowOk) * battery.voltageVolts;
                battery.energyFullWh = read(QStringLiteral("charge_full"), fullOk) * battery.voltageVolts;
                battery.energyDesignWh = read(QStringLiteral("charge_full_design"), designOk) * battery.voltageVolts;
            }
            battery.energyAvailable = nowOk && fullOk && battery.energyNowWh >= 0 && battery.energyFullWh > 0 && battery.energyNowWh <= battery.energyFullWh;
            battery.healthAvailable = fullOk && designOk && battery.energyFullWh > 0 && battery.energyDesignWh > 0;
            if (battery.healthAvailable) battery.healthPercent = std::clamp(100.0 * battery.energyFullWh / battery.energyDesignWh, 0.0, 100.0);
            // Charge ratios provide health even when voltage and energy are unavailable.
            if (!battery.healthAvailable) {
                bool chargeFullOk = false, chargeDesignOk = false;
                const auto full = read(QStringLiteral("charge_full"), chargeFullOk);
                const auto design = read(QStringLiteral("charge_full_design"), chargeDesignOk);
                if (chargeFullOk && chargeDesignOk && full > 0 && design > 0) {
                    battery.healthAvailable = true;
                    battery.healthPercent = std::clamp(100.0 * full / design, 0.0, 100.0);
                }
            }
            if (!battery.levelAvailable && battery.energyAvailable) {
                battery.percent = 100.0 * battery.energyNowWh / battery.energyFullWh;
                battery.levelAvailable = true;
            }
            if (battery.energyAvailable && battery.powerAvailable && battery.watts > 0.25) {
                const auto status = battery.status.toLower();
                const double remainingWh = status == QStringLiteral("charging") ? battery.energyFullWh - battery.energyNowWh : battery.energyNowWh;
                if (status == QStringLiteral("charging") || status == QStringLiteral("discharging"))
                    battery.timeRemainingMinutes = std::clamp(int(60.0 * remainingWh / battery.watts), 1, 2400);
            }
            snap.batteries.append(battery);
        }
    }
    int presentCount = 0;
    bool allEnergy = true, allPower = true, allHealthEnergy = true;
    bool charging = false, discharging = false;
    double nowWh = 0, fullWh = 0, designWh = 0, totalPower = 0;
    const BatteryTelemetry *single = nullptr;
    QString commonStatus;
    for (const auto &battery : snap.batteries) {
        if (!battery.present) continue;
        ++presentCount; single = &battery;
        allEnergy = allEnergy && battery.energyAvailable;
        allPower = allPower && battery.powerAvailable;
        allHealthEnergy = allHealthEnergy && battery.healthAvailable && battery.energyFullWh > 0 && battery.energyDesignWh > 0;
        nowWh += battery.energyNowWh; fullWh += battery.energyFullWh; designWh += battery.energyDesignWh;
        totalPower += battery.watts;
        charging = charging || battery.status.compare(QStringLiteral("charging"), Qt::CaseInsensitive) == 0;
        discharging = discharging || battery.status.compare(QStringLiteral("discharging"), Qt::CaseInsensitive) == 0;
        if (presentCount == 1) commonStatus = battery.status;
        else if (commonStatus != battery.status) commonStatus = QStringLiteral("Mixed");
    }
    snap.batteryPresent = presentCount > 0;
    snap.batteryStatus = commonStatus;
    if (externalKnown) { snap.batteryStateAvailable = true; snap.onAcPower = externalOnline; }
    else if (charging != discharging) { snap.batteryStateAvailable = true; snap.onAcPower = charging; }
    if (presentCount == 1) {
        snap.batteryLevelAvailable = single->levelAvailable;
        snap.batteryPercent = single->percent;
        snap.batteryHealthAvailable = single->healthAvailable;
        snap.batteryHealthPercent = single->healthPercent;
        snap.batteryVoltageVolts = single->voltageVolts;
        snap.batteryTimeRemainingMinutes = single->timeRemainingMinutes;
    } else if (presentCount > 1) {
        snap.batteryLevelAvailable = allEnergy;
        if (allEnergy && fullWh > 0) snap.batteryPercent = 100.0 * nowWh / fullWh;
        snap.batteryHealthAvailable = allHealthEnergy;
        if (allHealthEnergy && designWh > 0) snap.batteryHealthPercent = std::clamp(100.0 * fullWh / designWh, 0.0, 100.0);
        if (allEnergy && allPower && totalPower > 0.25 && charging != discharging)
            snap.batteryTimeRemainingMinutes = std::clamp(int(60.0 * (charging ? fullWh - nowWh : nowWh) / totalPower), 1, 2400);
    }
    snap.batteryPowerAvailable = presentCount > 0 && allPower && !(charging && discharging);
    if (snap.batteryPowerAvailable) snap.batteryWatts = totalPower;

    // Read Intel RAPL PL1/PL2 and energy_uj if readable
    const QDir raplDir(QDir(m_config.sysRoot).filePath(QStringLiteral("class/powercap/intel-rapl:0")));
    if (elapsedMs > 30000) {
        m_hasPrevRapl = false;
    }
    if (raplDir.exists()) {
        snap.pl1LimitWatts = SysfsReader::readScaledDouble(raplDir.filePath(QStringLiteral("constraint_0_power_limit_uw")), 1000000.0, 0.0);
        snap.pl2LimitWatts = SysfsReader::readScaledDouble(raplDir.filePath(QStringLiteral("constraint_1_power_limit_uw")), 1000000.0, 0.0);

        bool eOk = false;
        const qint64 energyRaw = SysfsReader::readInt64(raplDir.filePath(QStringLiteral("energy_uj")), -1, &eOk);
        bool rangeOk = false;
        const qint64 energyRangeRaw = SysfsReader::readInt64(raplDir.filePath(QStringLiteral("max_energy_range_uj")), -1, &rangeOk);
        if (eOk && energyRaw >= 0) {
            const quint64 curUj = static_cast<quint64>(energyRaw);
            quint64 deltaUj = 0;
            bool deltaAvailable = false;
            if (m_hasPrevRapl && elapsedMs > 50 && curUj >= m_prevRaplEnergyUj) {
                deltaUj = curUj - m_prevRaplEnergyUj;
                deltaAvailable = true;
            } else if (m_hasPrevRapl && elapsedMs > 50 && rangeOk && energyRangeRaw > 0
                       && m_prevRaplEnergyUj < static_cast<quint64>(energyRangeRaw)
                       && curUj < static_cast<quint64>(energyRangeRaw)) {
                deltaUj = static_cast<quint64>(energyRangeRaw) - m_prevRaplEnergyUj + curUj;
                deltaAvailable = true;
            }
            if (deltaAvailable) {
                const double dJoules = static_cast<double>(deltaUj) / 1000000.0;
                const double dtSec = static_cast<double>(elapsedMs) / 1000.0;
                snap.packageWatts = std::clamp(dJoules / dtSec, 0.0, 250.0);
                snap.raplExactAvailable = true;
                snap.packagePowerAvailable = true;
            }
            m_prevRaplEnergyUj = curUj;
            m_hasPrevRapl = true;
        } else {
            // Missing/invalid readings invalidate the previous counter interval. The next
            // successful value is a new baseline, not a one-poll power measurement.
            m_hasPrevRapl = false;
        }
    } else {
        m_hasPrevRapl = false;
    }

    // Scan hwmon for CPU package power (if RAPL unavailable) and fan*_input RPM
    const QDir hwmonRoot(QDir(m_config.sysRoot).filePath(QStringLiteral("class/hwmon")));
    if (hwmonRoot.exists()) {
        const QStringList hwmonDirs = hwmonRoot.entryList(QStringList{QStringLiteral("hwmon*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &dirName : hwmonDirs) {
            const QDir chipDir(hwmonRoot.filePath(dirName));
            const QString chipName = SysfsReader::readTrimmedLine(chipDir.filePath(QStringLiteral("name"))).toLower();
            const bool isCpuChip = chipName.contains(QStringLiteral("k10temp"))
                || chipName.contains(QStringLiteral("zenpower"))
                || chipName.contains(QStringLiteral("amd_energy"))
                || chipName.contains(QStringLiteral("fam15h_power"))
                || chipName.contains(QStringLiteral("coretemp"));

            if (!snap.raplExactAvailable && !snap.hwmonPowerAvailable && isCpuChip) {
                for (const QString &powerFile : {QStringLiteral("power1_average"), QStringLiteral("power1_input")}) {
                    bool pOk = false;
                    const double watts = SysfsReader::readScaledDouble(chipDir.filePath(powerFile), 1000000.0, 0.0, &pOk);
                    if (pOk && watts > 0.0 && watts <= 500.0) {
                        snap.packageWatts = watts;
                        snap.hwmonPowerAvailable = true;
                        snap.packagePowerAvailable = true;
                        snap.packagePowerEstimated = false;
                        break;
                    }
                }
            }

            const QStringList fanFiles = chipDir.entryList(QStringList{QStringLiteral("fan*_input")}, QDir::Files, QDir::Name);
            for (const QString &fanFile : fanFiles) {
                bool fOk = false;
                const int rpm = static_cast<int>(SysfsReader::readInt64(chipDir.filePath(fanFile), 0, &fOk));
                if (fOk) {
                    snap.fanAvailable = true;
                    snap.primaryFanRpm = std::max(snap.primaryFanRpm, rpm);
                }
            }
        }
    }

    // Fallback estimation if neither RAPL nor CPU hwmon power is readable
    if (!snap.raplExactAvailable && !snap.hwmonPowerAvailable && cpuLoadPercent >= 0.0 && cpuLoadPercent <= 100.0 && maxFreqMhz > 0 && avgFreqMhz > 0) {
        const double pl1 = (snap.pl1LimitWatts > 1.0) ? snap.pl1LimitWatts : 28.0;
        const double freqRatio = (maxFreqMhz > 0 && avgFreqMhz > 0)
            ? std::clamp(static_cast<double>(avgFreqMhz) / static_cast<double>(maxFreqMhz), 0.25, 1.0)
            : 0.5;
        const double loadFactor = std::clamp(cpuLoadPercent / 100.0, 0.02, 1.0);
        snap.packageWatts = std::clamp(2.2 + pl1 * loadFactor * freqRatio * 0.85, 1.5, pl1 * 1.4);
        snap.packagePowerAvailable = true;
        snap.packagePowerEstimated = true;
    }

    return snap;
}

} // namespace LoofiPulse
