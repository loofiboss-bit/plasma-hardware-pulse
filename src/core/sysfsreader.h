#pragma once

#include "telemetrytypes.h"

#include <QHash>
#include <QList>
#include <QString>

namespace LoofiPulse
{

class SysfsReader
{
public:
    explicit SysfsReader(SysfsRootConfig config = {});

    void setRootConfig(const SysfsRootConfig &config);
    const SysfsRootConfig &rootConfig() const;

    static QString readTrimmedLine(const QString &filePath, bool *ok = nullptr);
    static qint64 readInt64(const QString &filePath, qint64 fallback = 0, bool *ok = nullptr);
    static double readScaledDouble(const QString &filePath, double divisor = 1000.0, double fallback = 0.0, bool *ok = nullptr);

    QList<DomainSensorReading> sampleHwmonDomains(qint64 elapsedMs);
    static PhysicalDomain classifyChipDomain(const QString &chipName, const QString &label);
    static QString domainDisplayName(PhysicalDomain domain);

private:
    SysfsRootConfig m_config;
    QHash<QString, double> m_previousTemps;
    QHash<QString, double> m_smoothedVelocities;
};

} // namespace LoofiPulse
