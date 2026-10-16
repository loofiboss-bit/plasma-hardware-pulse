#include "sysfsreader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtMath>

#include <algorithm>

namespace LoofiPulse
{

SysfsReader::SysfsReader(SysfsRootConfig config)
    : m_config(std::move(config))
{
}

void SysfsReader::setRootConfig(const SysfsRootConfig &config)
{
    m_config = config;
    m_previousTemps.clear();
    m_smoothedVelocities.clear();
}

const SysfsRootConfig &SysfsReader::rootConfig() const
{
    return m_config;
}

QString SysfsReader::readTrimmedLine(const QString &filePath, bool *ok)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (ok) {
            *ok = false;
        }
        return {};
    }
    const QByteArray raw = file.read(256);
    if (ok) {
        *ok = !raw.isEmpty();
    }
    return QString::fromUtf8(raw).trimmed();
}

qint64 SysfsReader::readInt64(const QString &filePath, qint64 fallback, bool *ok)
{
    bool readOk = false;
    const QString text = readTrimmedLine(filePath, &readOk);
    if (!readOk || text.isEmpty()) {
        if (ok) {
            *ok = false;
        }
        return fallback;
    }
    bool parseOk = false;
    const qint64 value = text.toLongLong(&parseOk);
    if (ok) {
        *ok = parseOk;
    }
    return parseOk ? value : fallback;
}

double SysfsReader::readScaledDouble(const QString &filePath, double divisor, double fallback, bool *ok)
{
    bool intOk = false;
    const qint64 raw = readInt64(filePath, 0, &intOk);
    if (!intOk || qFuzzyIsNull(divisor)) {
        if (ok) {
            *ok = false;
        }
        return fallback;
    }
    if (ok) {
        *ok = true;
    }
    return static_cast<double>(raw) / divisor;
}

PhysicalDomain SysfsReader::classifyChipDomain(const QString &chipName, const QString &label)
{
    const QString lowerChip = chipName.toLower();
    const QString lowerLabel = label.toLower();

    if (lowerChip.contains(QStringLiteral("coretemp")) ||
        lowerChip.contains(QStringLiteral("k10temp")) ||
        lowerChip.contains(QStringLiteral("zenpower")) ||
        lowerChip.contains(QStringLiteral("cpu")) ||
        lowerLabel.contains(QStringLiteral("package")) ||
        lowerLabel.contains(QStringLiteral("core ")) ||
        lowerLabel.contains(QStringLiteral("tctl")) ||
        lowerLabel.contains(QStringLiteral("tdie"))) {
        return PhysicalDomain::CpuSoc;
    }

    if (lowerChip.contains(QStringLiteral("amdgpu")) ||
        lowerChip.contains(QStringLiteral("nouveau")) ||
        lowerChip.contains(QStringLiteral("i915")) ||
        lowerChip.contains(QStringLiteral("xe")) ||
        lowerLabel.contains(QStringLiteral("gpu")) ||
        lowerLabel.contains(QStringLiteral("edge")) ||
        lowerLabel.contains(QStringLiteral("junction"))) {
        return PhysicalDomain::Graphics;
    }

    if (lowerChip.contains(QStringLiteral("nvme")) ||
        lowerChip.contains(QStringLiteral("drivetemp")) ||
        lowerLabel.contains(QStringLiteral("composite"))) {
        return PhysicalDomain::StorageNvme;
    }

    if (lowerChip.contains(QStringLiteral("iwlwifi")) ||
        lowerChip.contains(QStringLiteral("mt79")) ||
        lowerChip.contains(QStringLiteral("ath")) ||
        lowerChip.contains(QStringLiteral("rtw"))) {
        return PhysicalDomain::NetworkWireless;
    }

    if (lowerChip.contains(QStringLiteral("bat")) ||
        lowerChip == QStringLiteral("ac") ||
        lowerChip.contains(QStringLiteral("ucsi"))) {
        return PhysicalDomain::PowerBattery;
    }

    return PhysicalDomain::ChassisCooling;
}

QString SysfsReader::domainDisplayName(PhysicalDomain domain)
{
    switch (domain) {
    case PhysicalDomain::CpuSoc:
        return QStringLiteral("CPU & SoC Package");
    case PhysicalDomain::Graphics:
        return QStringLiteral("Graphics & Display (GPU)");
    case PhysicalDomain::MemoryZram:
        return QStringLiteral("Memory & ZRAM Pool");
    case PhysicalDomain::StorageNvme:
        return QStringLiteral("Storage & NVMe");
    case PhysicalDomain::NetworkWireless:
        return QStringLiteral("Network & Wireless");
    case PhysicalDomain::PowerBattery:
        return QStringLiteral("Power & Battery");
    case PhysicalDomain::ChassisCooling:
        return QStringLiteral("Chassis & Cooling");
    }
    return QStringLiteral("System");
}

QList<DomainSensorReading> SysfsReader::sampleHwmonDomains(qint64 elapsedMs)
{
    QList<DomainSensorReading> readings;
    const QDir hwmonRoot(QDir(m_config.sysRoot).filePath(QStringLiteral("class/hwmon")));
    if (!hwmonRoot.exists()) {
        return readings;
    }

    const double dtSec = (elapsedMs > 50) ? (static_cast<double>(elapsedMs) / 1000.0) : 1.0;
    const QStringList hwmonDirs = hwmonRoot.entryList(QStringList{QStringLiteral("hwmon*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString &dirName : hwmonDirs) {
        const QDir chipDir(hwmonRoot.filePath(dirName));
        const QString chipName = readTrimmedLine(chipDir.filePath(QStringLiteral("name")));
        if (chipName.isEmpty()) {
            continue;
        }

        const QStringList tempInputs = chipDir.entryList(QStringList{QStringLiteral("temp*_input")}, QDir::Files, QDir::Name);
        for (const QString &inputFile : tempInputs) {
            bool tempOk = false;
            const double currentC = readScaledDouble(chipDir.filePath(inputFile), 1000.0, 0.0, &tempOk);
            if (!tempOk || currentC <= 0.5 || currentC > 150.0) {
                continue;
            }

            const QString prefix = inputFile.left(inputFile.indexOf(QLatin1Char('_')));
            QString label = readTrimmedLine(chipDir.filePath(prefix + QStringLiteral("_label")));
            if (label.isEmpty()) {
                label = QStringLiteral("%1 (%2)").arg(chipName, prefix);
            }

            bool maxOk = false;
            double maxC = readScaledDouble(chipDir.filePath(prefix + QStringLiteral("_max")), 1000.0, 0.0, &maxOk);
            bool critOk = false;
            double critC = readScaledDouble(chipDir.filePath(prefix + QStringLiteral("_crit")), 1000.0, 0.0, &critOk);

            if (!maxOk || maxC <= 10.0 || maxC > 150.0) {
                maxC = (critOk && critC > 10.0 && critC <= 150.0) ? critC : 95.0;
            }
            if (!critOk || critC <= 10.0 || critC > 150.0) {
                critC = maxC;
            }

            const QString sensorId = QStringLiteral("%1/%2:%3").arg(dirName, chipName, prefix);
            double velocity = 0.0;
            if (m_previousTemps.contains(sensorId)) {
                const double instVel = (currentC - m_previousTemps.value(sensorId)) / dtSec;
                const double prevSmoothed = m_smoothedVelocities.value(sensorId, 0.0);
                velocity = 0.35 * instVel + 0.65 * prevSmoothed;
            }
            m_previousTemps.insert(sensorId, currentC);
            m_smoothedVelocities.insert(sensorId, velocity);

            DomainSensorReading item;
            item.id = sensorId;
            item.chipName = chipName;
            item.label = label;
            item.domain = classifyChipDomain(chipName, label);
            item.currentCelsius = currentC;
            item.maxCelsius = maxC;
            item.critCelsius = critC;
            item.headroomCelsius = std::max(0.0, maxC - currentC);
            item.velocityCelsiusPerSec = velocity;
            readings.append(item);
        }
    }

    std::sort(readings.begin(), readings.end(), [](const DomainSensorReading &a, const DomainSensorReading &b) {
        if (a.domain != b.domain) {
            return static_cast<int>(a.domain) < static_cast<int>(b.domain);
        }
        return a.currentCelsius > b.currentCelsius;
    });

    return readings;
}

} // namespace LoofiPulse
