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
    if (psyRoot.exists()) {
        const QStringList entries = psyRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &entry : entries) {
            const QDir devDir(psyRoot.filePath(entry));
            const QString type = SysfsReader::readTrimmedLine(devDir.filePath(QStringLiteral("type"))).toLower();

            if (type == QStringLiteral("mains") || entry.startsWith(QStringLiteral("AC"))) {
                bool ok = false;
                const qint64 online = SysfsReader::readInt64(devDir.filePath(QStringLiteral("online")), 1, &ok);
                if (ok) {
                    snap.batteryStateAvailable = true;
                    snap.onAcPower = (online != 0);
                }
            } else if (type == QStringLiteral("battery") || entry.startsWith(QStringLiteral("BAT"))) {
                snap.batteryPresent = true;
                snap.batteryStatus = SysfsReader::readTrimmedLine(devDir.filePath(QStringLiteral("status")));
                const QString powerStatus = snap.batteryStatus.toLower();
                if (powerStatus == QStringLiteral("discharging")) {
                    snap.onAcPower = false;
                    snap.batteryStateAvailable = true;
                } else if (powerStatus == QStringLiteral("charging")) {
                    snap.onAcPower = true;
                    snap.batteryStateAvailable = true;
                }
                bool capacityOk = false;
                snap.batteryPercent = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("capacity")), 1.0, 0.0, &capacityOk);
                snap.batteryLevelAvailable = capacityOk && snap.batteryPercent >= 0.0 && snap.batteryPercent <= 100.0;

                bool vOk = false;
                const double voltageV = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("voltage_now")), 1000000.0, 0.0, &vOk);
                if (vOk) {
                    snap.batteryVoltageVolts = voltageV;
                }

                bool pOk = false;
                const double powerW = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("power_now")), 1000000.0, 0.0, &pOk);
                if (pOk && powerW >= 0.0) {
                    snap.batteryWatts = powerW;
                    snap.batteryPowerAvailable = true;
                } else {
                    bool cOk = false;
                    const double currentA = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("current_now")), 1000000.0, 0.0, &cOk);
                    if (cOk && vOk && currentA >= 0.0 && voltageV >= 0.0) {
                        snap.batteryWatts = currentA * voltageV;
                        snap.batteryPowerAvailable = true;
                    }
                }

                // Read energy_now / energy_full / energy_full_design (in uWh -> Wh) or fallback to charge_* (uAh)
                bool eNowOk = false;
                double energyNowWh = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("energy_now")), 1000000.0, 0.0, &eNowOk);
                double energyFullWh = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("energy_full")), 1000000.0, 0.0);
                double energyDesignWh = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("energy_full_design")), 1000000.0, 0.0);

                if (!eNowOk) {
                    const double effV = (voltageV > 1.0) ? voltageV : 11.4;
                    const double chargeNowAh = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("charge_now")), 1000000.0, 0.0, &eNowOk);
                    const double chargeFullAh = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("charge_full")), 1000000.0, 0.0);
                    const double chargeDesignAh = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("charge_full_design")), 1000000.0, 0.0);
                    if (eNowOk) {
                        energyNowWh = chargeNowAh * effV;
                        energyFullWh = chargeFullAh * effV;
                        energyDesignWh = chargeDesignAh * effV;
                    }
                }

                if (energyFullWh > 0.1 && energyDesignWh > 0.1) {
                    snap.batteryHealthPercent = std::clamp((energyFullWh * 100.0) / energyDesignWh, 10.0, 100.0);
                    snap.batteryHealthAvailable = true;
                }

                if (snap.batteryWatts > 0.25 && energyNowWh > 0.0) {
                    const QString lowerStatus = snap.batteryStatus.toLower();
                    if (lowerStatus.contains(QStringLiteral("discharging"))) {
                        snap.batteryTimeRemainingMinutes = std::clamp(
                            static_cast<int>((energyNowWh / snap.batteryWatts) * 60.0), 1, 2400);
                    } else if (lowerStatus.contains(QStringLiteral("charging")) && energyFullWh > energyNowWh) {
                        snap.batteryTimeRemainingMinutes = std::clamp(
                            static_cast<int>(((energyFullWh - energyNowWh) / snap.batteryWatts) * 60.0), 1, 2400);
                    }
                }
            }
        }
    }

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

    // Fallback estimation if RAPL energy_uj is root-only (0400)
    if (!snap.raplExactAvailable && cpuLoadPercent >= 0.0 && cpuLoadPercent <= 100.0 && maxFreqMhz > 0 && avgFreqMhz > 0) {
        const double pl1 = (snap.pl1LimitWatts > 1.0) ? snap.pl1LimitWatts : 28.0;
        const double freqRatio = (maxFreqMhz > 0 && avgFreqMhz > 0)
            ? std::clamp(static_cast<double>(avgFreqMhz) / static_cast<double>(maxFreqMhz), 0.25, 1.0)
            : 0.5;
        const double loadFactor = std::clamp(cpuLoadPercent / 100.0, 0.02, 1.0);
        snap.packageWatts = std::clamp(2.2 + pl1 * loadFactor * freqRatio * 0.85, 1.5, pl1 * 1.4);
        snap.packagePowerAvailable = true;
        snap.packagePowerEstimated = true;
    }

    // Scan hwmon for fan*_input RPM
    const QDir hwmonRoot(QDir(m_config.sysRoot).filePath(QStringLiteral("class/hwmon")));
    if (hwmonRoot.exists()) {
        const QStringList hwmonDirs = hwmonRoot.entryList(QStringList{QStringLiteral("hwmon*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &dirName : hwmonDirs) {
            const QDir chipDir(hwmonRoot.filePath(dirName));
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

    return snap;
}

} // namespace LoofiPulse
