#include "powerflowsampler.h"
#include "sysfsreader.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusVariant>
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
    snap.availablePowerProfiles = {
        QStringLiteral("power-saver"),
        QStringLiteral("balanced"),
        QStringLiteral("performance")
    };

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
                    snap.onAcPower = (online != 0);
                }
            } else if (type == QStringLiteral("battery") || entry.startsWith(QStringLiteral("BAT"))) {
                snap.batteryPresent = true;
                snap.batteryStatus = SysfsReader::readTrimmedLine(devDir.filePath(QStringLiteral("status")));
                snap.batteryPercent = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("capacity")), 1.0, 100.0);

                bool vOk = false;
                const double voltageV = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("voltage_now")), 1000000.0, 0.0, &vOk);
                if (vOk) {
                    snap.batteryVoltageVolts = voltageV;
                }

                bool pOk = false;
                const double powerW = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("power_now")), 1000000.0, 0.0, &pOk);
                if (pOk && powerW > 0.0) {
                    snap.batteryWatts = powerW;
                } else {
                    bool cOk = false;
                    const double currentA = SysfsReader::readScaledDouble(devDir.filePath(QStringLiteral("current_now")), 1000000.0, 0.0, &cOk);
                    if (cOk && vOk) {
                        snap.batteryWatts = currentA * voltageV;
                    }
                }
            }
        }
    }

    // Read Intel RAPL PL1/PL2 and energy_uj if readable
    const QDir raplDir(QDir(m_config.sysRoot).filePath(QStringLiteral("class/powercap/intel-rapl:0")));
    if (raplDir.exists()) {
        snap.pl1LimitWatts = SysfsReader::readScaledDouble(raplDir.filePath(QStringLiteral("constraint_0_power_limit_uw")), 1000000.0, 0.0);
        snap.pl2LimitWatts = SysfsReader::readScaledDouble(raplDir.filePath(QStringLiteral("constraint_1_power_limit_uw")), 1000000.0, 0.0);

        bool eOk = false;
        const qint64 energyRaw = SysfsReader::readInt64(raplDir.filePath(QStringLiteral("energy_uj")), -1, &eOk);
        if (eOk && energyRaw >= 0) {
            const quint64 curUj = static_cast<quint64>(energyRaw);
            if (m_hasPrevRapl && curUj >= m_prevRaplEnergyUj && elapsedMs > 50) {
                const double dJoules = static_cast<double>(curUj - m_prevRaplEnergyUj) / 1000000.0;
                const double dtSec = static_cast<double>(elapsedMs) / 1000.0;
                snap.packageWatts = std::clamp(dJoules / dtSec, 0.0, 250.0);
                snap.raplExactAvailable = true;
            }
            m_prevRaplEnergyUj = curUj;
            m_hasPrevRapl = true;
        }
    }

    // Fallback estimation if RAPL energy_uj is root-only (0400)
    if (!snap.raplExactAvailable) {
        const double pl1 = (snap.pl1LimitWatts > 1.0) ? snap.pl1LimitWatts : 28.0;
        const double freqRatio = (maxFreqMhz > 0 && avgFreqMhz > 0)
            ? std::clamp(static_cast<double>(avgFreqMhz) / static_cast<double>(maxFreqMhz), 0.25, 1.0)
            : 0.5;
        const double loadFactor = std::clamp(cpuLoadPercent / 100.0, 0.02, 1.0);
        snap.packageWatts = std::clamp(2.2 + pl1 * loadFactor * freqRatio * 0.85, 1.5, pl1 * 1.4);
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
                if (fOk && rpm > snap.primaryFanRpm) {
                    snap.primaryFanRpm = rpm;
                }
            }
        }
    }

    // Query active Power Profile via D-Bus if using live /sys
    if (m_config.sysRoot == QStringLiteral("/sys") && QDBusConnection::systemBus().isConnected()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("net.hadess.PowerProfiles"),
            QStringLiteral("/net/hadess/PowerProfiles"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("Get"));
        msg << QStringLiteral("net.hadess.PowerProfiles") << QStringLiteral("ActiveProfile");
        const QDBusMessage reply = QDBusConnection::systemBus().call(msg, QDBus::Block, 50);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            const QVariant unwrapped = reply.arguments().at(0).value<QDBusVariant>().variant();
            const QString prof = unwrapped.toString().trimmed();
            if (!prof.isEmpty()) {
                snap.activePowerProfile = prof;
            }
        }
    }

    return snap;
}

bool PowerFlowSampler::setSystemPowerProfile(const QString &profile)
{
    if (!QDBusConnection::systemBus().isConnected()) {
        return false;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("net.hadess.PowerProfiles"),
        QStringLiteral("/net/hadess/PowerProfiles"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Set"));
    msg << QStringLiteral("net.hadess.PowerProfiles")
        << QStringLiteral("ActiveProfile")
        << QVariant::fromValue(QDBusVariant(profile));

    const QDBusMessage reply = QDBusConnection::systemBus().call(msg, QDBus::Block, 300);
    return reply.type() == QDBusMessage::ReplyMessage;
}

} // namespace LoofiPulse
