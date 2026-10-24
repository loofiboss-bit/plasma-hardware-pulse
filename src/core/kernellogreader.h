#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace LoofiPulse
{

struct KernelLogEntry
{
    QString timestamp;
    qint64 epochMs = 0;
    QString subsystem;
    QString message;
    QString severity; // "error", "warning", "info"
};

class KernelLogReader
{
public:
    static QString redactSensitiveData(const QString &text);
    static bool isHardwareRelevant(const QString &text);
    static QList<KernelLogEntry> parseJournalLines(const QString &rawOutput);
    static QList<KernelLogEntry> fetchIncidentLogs(qint64 startMs, qint64 endMs, qint64 marginMs = 15000, const QString &mockOutput = QString());
    static QVariantList fetchIncidentLogsVariant(qint64 startMs, qint64 endMs, qint64 marginMs = 15000, const QString &mockOutput = QString());
};

} // namespace LoofiPulse
