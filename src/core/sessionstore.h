#pragma once
#include "telemetrytypes.h"
#include <QByteArray>
namespace LoofiPulse {
struct SessionSnapshot {
    QString recordingId;
    QString engineVersion = QStringLiteral("0.9.0");
    QList<FlightSample> samples;
    QVariantList incidents;
    QVariantList markers;
    QVariantMap metadata;
};
class SessionStore {
public:
    static constexpr qint64 MaximumBytes = 64 * 1024 * 1024;
    static constexpr int MaximumSamples = 7201;
    static QVariantMap sampleToMap(const FlightSample &sample, bool includeProcessDetails = true);
    static bool sampleFromMap(const QVariantMap &map, FlightSample *sample, QString *error = nullptr);
    static QByteArray encode(const SessionSnapshot &snapshot, bool includeProcessDetails = false, QString *error = nullptr);
    static bool decode(const QByteArray &data, SessionSnapshot *snapshot, QString *error = nullptr);
    static bool write(const QString &path, const SessionSnapshot &snapshot, bool includeProcessDetails = false, QString *error = nullptr);
    static bool read(const QString &path, SessionSnapshot *snapshot, QString *error = nullptr);
    static QVariant redactProcessDetails(const QVariant &value);
    static QVariant redactReportText(const QVariant &value, const QList<FlightSample> &recording);
};
} // namespace LoofiPulse
