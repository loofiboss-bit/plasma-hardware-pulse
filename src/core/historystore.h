#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <utility>

namespace LoofiPulse
{

// Opt-in persistent long-term history: 1-minute aggregates of key metrics, incidents and
// recording gaps in SQLite. Never stores process names, PIDs or owners.
// Not thread-safe: use from a single thread (the collector worker thread).
class HistoryStore
{
public:
    struct MetricSpec { QString id; QString valueKey; QString availableKey; };
    static const QList<MetricSpec> &metrics();

    explicit HistoryStore();
    ~HistoryStore();
    HistoryStore(const HistoryStore &) = delete;
    HistoryStore &operator=(const HistoryStore &) = delete;

    bool open(const QString &path, qint64 retentionMs, qint64 maxBytes = 64ll * 1024 * 1024);
    void close(); // flushes the current partial minute (best effort) before closing
    bool isOpen() const;

    void setRetentionMs(qint64 ms); // clamped to [1h, 31d]
    qint64 retentionMs() const;
    void setMaxBytes(qint64 bytes); // clamped to [256 KiB, 16 GiB]
    qint64 maxBytes() const;

    void addSample(qint64 timestampMs, const QVariantMap &live);
    bool flush(bool includePartialMinute = false);
    void recordGap(qint64 startMs, qint64 endMs, const QString &reason);
    void recordIncident(const QVariantMap &incident);

    QVariantMap trends(qint64 fromMs, qint64 toMs, int maxPoints, const QStringList &metricIds) const;
    QVariantList incidents(qint64 fromMs, qint64 toMs, int limit) const;
    bool clear();

    qint64 sizeBytes() const;
    QString path() const;
    QString lastError() const;
    QString recoveryNotice() const;
    qint64 oldestMs() const; // 0 when empty
    qint64 newestMs() const; // 0 when empty
    QString connectionName() const;

private:
    struct Agg { double min = 0; double max = 0; double sum = 0; qint64 n = 0; };
    using Bucket = QMap<QString, Agg>;

    bool openConnection(bool *corrupt);
    void closeConnection();
    bool setupSchema();
    bool enforceLimits();
    void reclaimFreePages();
    qint64 pragmaInt(const QString &name) const;
    bool fail(const QString &message);
    void queueBucket(qint64 minute, const Bucket &bucket);

    QString m_connection;
    QString m_path;
    QString m_lastError;
    QString m_recoveryNotice;
    qint64 m_retentionMs = 7ll * 24 * 3600 * 1000;
    qint64 m_maxBytes = 64ll * 1024 * 1024;
    qint64 m_currentMinute = -1;
    qint64 m_latestMs = 0;
    Bucket m_current;
    QList<std::pair<qint64, Bucket>> m_queued;
    bool m_open = false;
};

} // namespace LoofiPulse
