#include "historystore.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <cmath>

namespace LoofiPulse
{

const QList<HistoryStore::MetricSpec> &HistoryStore::metrics()
{
    static const QList<MetricSpec> s_metrics = {
        {QStringLiteral("cpuLoad"), QStringLiteral("cpuLoadPercent"), QStringLiteral("cpuLoadAvailable")},
        {QStringLiteral("cpuTemp"), QStringLiteral("cpuPackageTemp"), QStringLiteral("cpuTempAvailable")},
        {QStringLiteral("cpuFreq"), QStringLiteral("cpuAvgFreqMhz"), QString()},
        {QStringLiteral("gpuBusy"), QStringLiteral("gpuBusyPercent"), QStringLiteral("gpuBusyAvailable")},
        {QStringLiteral("gpuTemp"), QStringLiteral("gpuTempCelsius"), QStringLiteral("gpuTempAvailable")},
        {QStringLiteral("gpuPower"), QStringLiteral("gpuPowerWatts"), QStringLiteral("gpuPowerAvailable")},
        {QStringLiteral("psiCpu"), QStringLiteral("psiCpuSome10"), QStringLiteral("psiCpuAvailable")},
        {QStringLiteral("psiMem"), QStringLiteral("psiMemSome10"), QStringLiteral("psiMemoryAvailable")},
        {QStringLiteral("psiIo"), QStringLiteral("psiIoSome10"), QStringLiteral("psiIoAvailable")},
        {QStringLiteral("responsiveness"), QStringLiteral("responsivenessScore"), QStringLiteral("psiAvailable")},
        {QStringLiteral("ramUsed"), QStringLiteral("ramUsedPercent"), QStringLiteral("memoryAvailable")},
        {QStringLiteral("swapUsed"), QStringLiteral("swapUsedGib"), QStringLiteral("swapAvailable")},
        {QStringLiteral("diskRead"), QStringLiteral("diskReadMibPerSec"), QStringLiteral("diskThroughputAvailable")},
        {QStringLiteral("diskWrite"), QStringLiteral("diskWriteMibPerSec"), QStringLiteral("diskThroughputAvailable")},
        {QStringLiteral("netRx"), QStringLiteral("netRxMibPerSec"), QStringLiteral("netThroughputAvailable")},
        {QStringLiteral("netTx"), QStringLiteral("netTxMibPerSec"), QStringLiteral("netThroughputAvailable")},
        {QStringLiteral("packagePower"), QStringLiteral("packageWatts"), QStringLiteral("packagePowerAvailable")},
        {QStringLiteral("batteryLevel"), QStringLiteral("batteryPercent"), QStringLiteral("batteryLevelAvailable")},
        {QStringLiteral("batteryPower"), QStringLiteral("batteryWatts"), QStringLiteral("batteryPowerAvailable")},
        {QStringLiteral("storageTemp"), QStringLiteral("nvmeTempCelsius"), QStringLiteral("nvmeTempAvailable")},
        {QStringLiteral("fan"), QStringLiteral("primaryFanRpm"), QStringLiteral("fanAvailable")},
        {QStringLiteral("batteryHealth"), QStringLiteral("batteryHealthPercent"), QStringLiteral("batteryHealthAvailable")},
        {QStringLiteral("zramRatio"), QStringLiteral("zramCompressionRatio"), QStringLiteral("zramCompressionAvailable")},
        {QStringLiteral("wifiSignal"), QStringLiteral("wifiSignalPercent"), QStringLiteral("wifiAvailable")}
    };
    return s_metrics;
}

HistoryStore::HistoryStore()
    : m_connection(QStringLiteral("LoofiPulse_HistoryStore_%1").arg(reinterpret_cast<quintptr>(this), 0, 16))
{
}

HistoryStore::~HistoryStore()
{
    close();
}

bool HistoryStore::open(const QString &path, qint64 retentionMs, qint64 maxBytes)
{
    close();
    m_path = path;
    setRetentionMs(retentionMs);
    setMaxBytes(maxBytes);
    m_lastError.clear();
    m_recoveryNotice.clear();

    const QFileInfo fi(path);
    if (!fi.dir().exists() && !QDir().mkpath(fi.dir().absolutePath())) {
        return fail(QStringLiteral("Cannot create directory for history store: %1").arg(fi.dir().absolutePath()));
    }

    bool corrupt = false;
    if (!openConnection(&corrupt)) {
        if (corrupt) {
            closeConnection();
            const QString timestamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmss"));
            const QString corruptPath = QStringLiteral("%1.corrupt-%2.sqlite").arg(path, timestamp);
            if (!QFile::rename(path, corruptPath)) return fail(QStringLiteral("Cannot preserve corrupted history database."));
            QFile::rename(path + QStringLiteral("-wal"), corruptPath + QStringLiteral("-wal"));
            QFile::rename(path + QStringLiteral("-shm"), corruptPath + QStringLiteral("-shm"));

            if (openConnection(&corrupt) && setupSchema()) {
                m_open = true;
                m_recoveryNotice = QStringLiteral("Corrupted history database recovered to %1").arg(corruptPath);
                return true;
            }
        }
        return false;
    }

    if (!setupSchema()) {
        closeConnection();
        return false;
    }

    m_open = true;
    return true;
}

void HistoryStore::close()
{
    if (m_open) flush(true);
    closeConnection();
    m_open = false;
    m_queued.clear();
    m_current.clear();
    m_currentMinute = -1;
    m_latestMs = 0;
}

bool HistoryStore::isOpen() const
{
    return m_open;
}

void HistoryStore::setRetentionMs(qint64 ms)
{
    m_retentionMs = std::clamp<qint64>(ms, 3600000LL, 31LL * 86400000LL);
}

qint64 HistoryStore::retentionMs() const
{
    return m_retentionMs;
}

void HistoryStore::setMaxBytes(qint64 bytes)
{
    m_maxBytes = std::clamp<qint64>(bytes, 256LL * 1024LL, 16LL * 1024LL * 1024LL * 1024LL);
}

qint64 HistoryStore::maxBytes() const
{
    return m_maxBytes;
}

void HistoryStore::addSample(qint64 timestampMs, const QVariantMap &live)
{
    if (!m_open || timestampMs <= 0) return;
    const qint64 minute = (timestampMs / 60000LL) * 60000LL;
    if (m_currentMinute != -1 && minute > m_currentMinute) {
        queueBucket(m_currentMinute, m_current);
        m_current.clear();
        m_currentMinute = minute;
    } else if (m_currentMinute == -1) {
        // Resume a partial minute saved by an earlier collector lifecycle. Seed only
        // once: subsequent partial flushes replace this complete aggregate.
        QSqlQuery q(QSqlDatabase::database(m_connection));
        q.prepare(QStringLiteral("SELECT metric, min, avg, max, n FROM minute_agg WHERE ts_min = ?;"));
        q.addBindValue(minute);
        if (!q.exec()) {
            fail(QStringLiteral("Failed to resume history minute: %1").arg(q.lastError().text()));
            return;
        }
        Bucket resumed;
        while (q.next()) {
            const qint64 n = q.value(4).toLongLong();
            if (n > 0) {
                resumed.insert(q.value(0).toString(),
                               Agg{q.value(1).toDouble(), q.value(3).toDouble(),
                                   q.value(2).toDouble() * static_cast<double>(n), n});
            }
        }
        if (q.lastError().isValid()) {
            fail(QStringLiteral("Failed to resume history minute: %1").arg(q.lastError().text()));
            return;
        }
        m_current = std::move(resumed);
        m_currentMinute = minute;
    }
    m_latestMs = std::max(m_latestMs, timestampMs);

    const auto &specs = metrics();
    for (const auto &spec : specs) {
        bool available = false;
        if (!spec.availableKey.isEmpty()) {
            available = live.value(spec.availableKey).toBool();
        } else {
            const QVariant v = live.value(spec.valueKey);
            bool ok = false;
            const double num = v.toDouble(&ok);
            available = ok && std::isfinite(num) && num > 0.0;
        }

        if (!available) continue;

        bool ok = false;
        const double val = live.value(spec.valueKey).toDouble(&ok);
        if (!ok || !std::isfinite(val)) continue;

        auto &agg = m_current[spec.id];
        if (agg.n == 0) {
            agg.min = val;
            agg.max = val;
            agg.sum = val;
            agg.n = 1;
        } else {
            agg.min = std::min(agg.min, val);
            agg.max = std::max(agg.max, val);
            agg.sum += val;
            agg.n += 1;
        }
    }
}

void HistoryStore::queueBucket(qint64 minute, const Bucket &bucket)
{
    if (bucket.isEmpty()) return;
    m_queued.append({minute, bucket});
    while (m_queued.size() > 120) {
        m_queued.removeFirst();
    }
}

bool HistoryStore::flush(bool includePartialMinute)
{
    if (!m_open) return false;
    auto db = QSqlDatabase::database(m_connection);
    if (!db.isOpen()) return fail(QStringLiteral("Database is not open"));

    if (m_queued.isEmpty() && (!includePartialMinute || m_current.isEmpty())) {
        return true;
    }

    if (!db.transaction()) {
        return fail(QStringLiteral("Failed to begin transaction: %1").arg(db.lastError().text()));
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO minute_agg (ts_min, metric, min, avg, max, n) VALUES (?, ?, ?, ?, ?, ?);"));

    for (const auto &item : std::as_const(m_queued)) {
        const qint64 minTs = item.first;
        for (auto it = item.second.cbegin(); it != item.second.cend(); ++it) {
            const auto &agg = it.value();
            if (agg.n <= 0) continue;
            q.addBindValue(minTs);
            q.addBindValue(it.key());
            q.addBindValue(agg.min);
            q.addBindValue(agg.sum / static_cast<double>(agg.n));
            q.addBindValue(agg.max);
            q.addBindValue(agg.n);
            if (!q.exec()) {
                db.rollback();
                return fail(QStringLiteral("Failed to insert aggregate: %1").arg(q.lastError().text()));
            }
        }
    }

    if (includePartialMinute && !m_current.isEmpty() && m_currentMinute >= 0) {
        for (auto it = m_current.cbegin(); it != m_current.cend(); ++it) {
            const auto &agg = it.value();
            if (agg.n <= 0) continue;
            q.addBindValue(m_currentMinute);
            q.addBindValue(it.key());
            q.addBindValue(agg.min);
            q.addBindValue(agg.sum / static_cast<double>(agg.n));
            q.addBindValue(agg.max);
            q.addBindValue(agg.n);
            if (!q.exec()) {
                db.rollback();
                return fail(QStringLiteral("Failed to insert partial minute: %1").arg(q.lastError().text()));
            }
        }
    }

    // Retention prune
    const qint64 cutoff = m_latestMs > 0 ? (m_latestMs - m_retentionMs) : 0;
    if (cutoff > 0) {
        QSqlQuery pruneQ(db);
        pruneQ.prepare(QStringLiteral("DELETE FROM minute_agg WHERE ts_min < ?;"));
        pruneQ.addBindValue(cutoff);
        if (!pruneQ.exec()) { db.rollback(); return fail(pruneQ.lastError().text()); }

        pruneQ.prepare(QStringLiteral("DELETE FROM incidents WHERE calendar_time_known = 1 AND end_ms < ?;"));
        pruneQ.addBindValue(cutoff);
        if (!pruneQ.exec()) { db.rollback(); return fail(pruneQ.lastError().text()); }

        pruneQ.prepare(QStringLiteral("DELETE FROM gaps WHERE end_ms < ?;"));
        pruneQ.addBindValue(cutoff);
        if (!pruneQ.exec()) { db.rollback(); return fail(pruneQ.lastError().text()); }
    }

    if (!enforceLimits()) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        db.rollback();
        return fail(QStringLiteral("Failed to commit transaction: %1").arg(db.lastError().text()));
    }

    m_queued.clear();
    if (!reclaimFreePages()) return false;
    m_lastError.clear();
    return true;
}

bool HistoryStore::enforceLimits()
{
    auto db = QSqlDatabase::database(m_connection);
    // SQLite DELETE frees pages logically; physical size cannot shrink until after commit.
    const qint64 pageSize = pragmaInt(QStringLiteral("page_size"));
    qint64 used = (pragmaInt(QStringLiteral("page_count")) - pragmaInt(QStringLiteral("freelist_count"))) * pageSize;
    for (int batch = 0; used > m_maxBytes && batch < 32; ++batch) {
        QSqlQuery boundary(db);
        if (!boundary.exec(QStringLiteral("SELECT ts_min FROM minute_agg GROUP BY ts_min ORDER BY ts_min LIMIT 1 OFFSET 59;")))
            return fail(boundary.lastError().text());
        if (!boundary.next()) break; // Always preserve the newest partial hour.
        const qint64 cutoff = boundary.value(0).toLongLong();
        boundary.finish();
        QSqlQuery newest(db);
        if (!newest.exec(QStringLiteral("SELECT MAX(ts_min) FROM minute_agg;")) || !newest.next())
            return fail(newest.lastError().text());
        if (cutoff >= newest.value(0).toLongLong()) break;
        newest.finish();
        for (const QString &sql : {QStringLiteral("DELETE FROM minute_agg WHERE ts_min <= ?;"),
                                  QStringLiteral("DELETE FROM incidents WHERE calendar_time_known = 1 AND end_ms <= ?;"),
                                  QStringLiteral("DELETE FROM gaps WHERE end_ms <= ?;")}) {
            QSqlQuery del(db);
            del.prepare(sql); del.addBindValue(cutoff);
            if (!del.exec()) return fail(del.lastError().text());
        }
        m_reclaimNeeded = true;
        used = (pragmaInt(QStringLiteral("page_count")) - pragmaInt(QStringLiteral("freelist_count"))) * pageSize;
    }
    // Commit bounded progress even if an unusually large database needs another maintenance pass.
    // reclaimFreePages reports an outstanding physical limit after preserving recent/legacy records.
    return true;
}

qint64 HistoryStore::pragmaInt(const QString &name) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    if (!q.exec(QStringLiteral("PRAGMA %1;").arg(name)) || !q.next()) return 0;
    return q.value(0).toLongLong();
}

bool HistoryStore::reclaimFreePages()
{
    if (!m_reclaimNeeded && sizeBytes() <= m_maxBytes) return true;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    if (!q.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE);")) || !q.next() || q.value(0).toInt() != 0)
        return fail(QStringLiteral("History checkpoint could not complete."));
    q.finish();
    if (!q.exec(QStringLiteral("VACUUM;"))) return fail(q.lastError().text());
    if (!q.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE);")) || !q.next() || q.value(0).toInt() != 0)
        return fail(QStringLiteral("History checkpoint could not complete."));
    m_reclaimNeeded = false;
    if (sizeBytes() > m_maxBytes) return fail(QStringLiteral("History files exceed the configured storage limit."));
    return true;
}

bool HistoryStore::recordGap(qint64 startMs, qint64 endMs, const QString &reason)
{
    if (!m_open || endMs <= startMs) return false;
    auto db = QSqlDatabase::database(m_connection);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO gaps (start_ms, end_ms, reason) VALUES (?, ?, ?);"));
    q.addBindValue(startMs);
    q.addBindValue(endMs);
    q.addBindValue(reason.trimmed().isEmpty() ? QStringLiteral("collector-stopped") : reason.trimmed());
    if (!q.exec()) return fail(q.lastError().text());
    return true;
}

bool HistoryStore::recordIncident(const QVariantMap &incident)
{
    if (!m_open) return false;
    const QString id = incident.value(QStringLiteral("incidentId")).toString();
    if (id.isEmpty()) return false;

    auto db = QSqlDatabase::database(m_connection);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO incidents (id, start_ms, end_ms, severity, rule_id, headline, domain, calendar_time_known) VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "start_ms=CASE WHEN excluded.calendar_time_known=1 OR incidents.calendar_time_known=0 THEN excluded.start_ms ELSE incidents.start_ms END, "
        "end_ms=CASE WHEN excluded.calendar_time_known=1 OR incidents.calendar_time_known=0 THEN excluded.end_ms ELSE incidents.end_ms END, "
        "calendar_time_known=MAX(incidents.calendar_time_known, excluded.calendar_time_known), "
        "severity=excluded.severity, rule_id=excluded.rule_id, headline=excluded.headline, domain=excluded.domain;"));
    q.addBindValue(id);
    q.addBindValue(incident.value(QStringLiteral("startMs")).toLongLong());
    q.addBindValue(incident.value(QStringLiteral("endMs")).toLongLong());
    q.addBindValue(incident.value(QStringLiteral("severity")).toInt());
    q.addBindValue(incident.value(QStringLiteral("ruleId")).toString());
    q.addBindValue(incident.value(QStringLiteral("headline")).toString());
    q.addBindValue(incident.value(QStringLiteral("domain")).toString());
    q.addBindValue(incident.value(QStringLiteral("calendarTimeKnown"), true).toBool() ? 1 : 0);
    if (!q.exec()) return fail(q.lastError().text());
    return true;
}

bool HistoryStore::recordIncident(const QVariantMap &incident, const QMap<qulonglong, qint64> &sampleWallTimes)
{
    auto persistent = incident;
    const auto startId = incident.value(QStringLiteral("startSampleId")).toULongLong();
    const auto endId = incident.value(QStringLiteral("endSampleId")).toULongLong();
    qint64 startMs = sampleWallTimes.value(startId, 0);
    if (!startMs && m_open) {
        QSqlQuery existing(QSqlDatabase::database(m_connection));
        existing.prepare(QStringLiteral("SELECT start_ms FROM incidents WHERE id = ? AND calendar_time_known = 1;"));
        existing.addBindValue(incident.value(QStringLiteral("incidentId")));
        if (existing.exec() && existing.next()) startMs = existing.value(0).toLongLong();
    }
    const bool known = startMs > 0 && sampleWallTimes.contains(endId);
    persistent[QStringLiteral("calendarTimeKnown")] = known;
    if (known) {
        persistent[QStringLiteral("startMs")] = startMs;
        persistent[QStringLiteral("endMs")] = sampleWallTimes.value(endId);
    }
    return recordIncident(persistent);
}

QVariantMap HistoryStore::trends(qint64 fromMs, qint64 toMs, int maxPoints, const QStringList &metricIds, QString *error) const
{
    if (error) error->clear();
    QVariantMap result{{QStringLiteral("storageError"), m_lastError},
                       {QStringLiteral("calendarTimeUnknownIncidents"), legacyIncidentCount()}};
    if (!m_open || toMs <= fromMs) return result;
    const auto readFailure = [&](const QSqlError &sqlError) {
        if (error) *error = sqlError.text();
        return QVariantMap{{QStringLiteral("storageError"), sqlError.text()}};
    };

    const int clampedPoints = std::clamp(maxPoints, 10, 2000);
    const qint64 span = toMs - fromMs;
    const qint64 rawBucket = (span + clampedPoints - 1) / clampedPoints;
    const qint64 bucketMs = std::max<qint64>(60000LL, ((rawBucket + 59999LL) / 60000LL) * 60000LL);

    result[QStringLiteral("fromMs")] = fromMs;
    result[QStringLiteral("toMs")] = toMs;
    result[QStringLiteral("bucketMs")] = bucketMs;

    auto db = QSqlDatabase::database(m_connection);

    // Build metric list
    QSet<QString> validMetricIds;
    for (const auto &spec : metrics()) validMetricIds.insert(spec.id);

    QStringList requested;
    for (const auto &id : metricIds) {
        if (validMetricIds.contains(id)) requested.append(id);
    }
    if (requested.isEmpty()) {
        for (const auto &spec : metrics()) requested.append(spec.id);
    }

    QVariantMap series;
    for (const auto &metricId : requested) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT (ts_min / :bucketMs) * :bucketMs AS b_ts, "
            "       MIN(min) AS b_min, "
            "       SUM(avg * n) / SUM(n) AS b_avg, "
            "       MAX(max) AS b_max, "
            "       SUM(n) AS b_n "
            "FROM minute_agg "
            "WHERE ts_min >= :fromMs AND ts_min <= :toMs AND metric = :metric "
            "GROUP BY b_ts "
            "ORDER BY b_ts ASC;"));
        q.bindValue(QStringLiteral(":bucketMs"), bucketMs);
        q.bindValue(QStringLiteral(":fromMs"), fromMs);
        q.bindValue(QStringLiteral(":toMs"), toMs);
        q.bindValue(QStringLiteral(":metric"), metricId);

        QVariantList points;
        if (q.exec()) {
            while (q.next()) {
                QVariantMap pt;
                pt[QStringLiteral("t")] = q.value(0).toLongLong();
                pt[QStringLiteral("min")] = q.value(1).toDouble();
                pt[QStringLiteral("avg")] = q.value(2).toDouble();
                pt[QStringLiteral("max")] = q.value(3).toDouble();
                pt[QStringLiteral("n")] = q.value(4).toLongLong();
                points.append(pt);
            }
        }
        if (q.lastError().isValid()) return readFailure(q.lastError());
        series[metricId] = points;
    }
    result[QStringLiteral("series")] = series;

    // Gaps
    QVariantList gaps;
    QSqlQuery gapQ(db);
    gapQ.prepare(QStringLiteral("SELECT start_ms, end_ms, reason FROM gaps WHERE end_ms >= ? AND start_ms <= ? ORDER BY start_ms ASC;"));
    gapQ.addBindValue(fromMs);
    gapQ.addBindValue(toMs);
    if (gapQ.exec()) {
        while (gapQ.next()) {
            QVariantMap g;
            g[QStringLiteral("startMs")] = gapQ.value(0).toLongLong();
            g[QStringLiteral("endMs")] = gapQ.value(1).toLongLong();
            g[QStringLiteral("reason")] = gapQ.value(2).toString();
            gaps.append(g);
        }
    }
    if (gapQ.lastError().isValid()) return readFailure(gapQ.lastError());
    result[QStringLiteral("gaps")] = gaps;

    // Coverage calculation
    QSqlQuery covQ(db);
    covQ.prepare(QStringLiteral("SELECT COUNT(DISTINCT ts_min) FROM minute_agg WHERE ts_min >= ? AND ts_min <= ?;"));
    covQ.addBindValue(fromMs);
    covQ.addBindValue(toMs);
    qint64 coverageMinutes = 0;
    if (covQ.exec() && covQ.next()) {
        coverageMinutes = covQ.value(0).toLongLong();
    }
    if (covQ.lastError().isValid()) return readFailure(covQ.lastError());
    result[QStringLiteral("coverageMs")] = coverageMinutes * 60000LL;

    return result;
}

QVariantList HistoryStore::incidents(qint64 fromMs, qint64 toMs, int limit, QString *error) const
{
    if (error) error->clear();
    QVariantList list;
    if (!m_open) return list;

    const int clampedLimit = std::clamp(limit, 1, 500);
    auto db = QSqlDatabase::database(m_connection);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT id, start_ms, end_ms, severity, rule_id, headline, domain FROM incidents WHERE calendar_time_known = 1 AND end_ms >= ? AND start_ms <= ? ORDER BY start_ms DESC LIMIT ?;"));
    q.addBindValue(fromMs);
    q.addBindValue(toMs);
    q.addBindValue(clampedLimit);
    if (q.exec()) {
        while (q.next()) {
            QVariantMap inc;
            inc[QStringLiteral("incidentId")] = q.value(0).toString();
            inc[QStringLiteral("startMs")] = q.value(1).toLongLong();
            inc[QStringLiteral("endMs")] = q.value(2).toLongLong();
            inc[QStringLiteral("severity")] = q.value(3).toInt();
            inc[QStringLiteral("ruleId")] = q.value(4).toString();
            inc[QStringLiteral("headline")] = q.value(5).toString();
            inc[QStringLiteral("domain")] = q.value(6).toString();
            inc[QStringLiteral("calendarTimeKnown")] = true;
            list.append(inc);
        }
    }
    if (q.lastError().isValid()) {
        if (error) *error = q.lastError().text();
        return {};
    }
    return list;
}

bool HistoryStore::clear()
{
    if (!m_open) return false;
    auto db = QSqlDatabase::database(m_connection);
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("DELETE FROM minute_agg;")) ||
        !q.exec(QStringLiteral("DELETE FROM incidents;")) ||
        !q.exec(QStringLiteral("DELETE FROM gaps;")) ||
        !q.exec(QStringLiteral("VACUUM;"))) {
        return fail(QStringLiteral("Failed to clear database: %1").arg(q.lastError().text()));
    }
    m_queued.clear();
    m_current.clear();
    m_currentMinute = -1;
    m_latestMs = 0;
    m_lastError.clear();
    return true;
}

qint64 HistoryStore::sizeBytes() const
{
    if (m_path.isEmpty()) return 0;
    qint64 total = QFileInfo(m_path).size();
    const QFileInfo wal(m_path + QStringLiteral("-wal"));
    if (wal.exists()) total += wal.size();
    const QFileInfo shm(m_path + QStringLiteral("-shm"));
    if (shm.exists()) total += shm.size();
    return total;
}

QString HistoryStore::path() const
{
    return m_path;
}

QString HistoryStore::lastError() const
{
    return m_lastError;
}

QString HistoryStore::recoveryNotice() const
{
    return m_recoveryNotice;
}

qint64 HistoryStore::oldestMs() const
{
    if (!m_open) return 0;
    auto db = QSqlDatabase::database(m_connection);
    QSqlQuery q(QStringLiteral("SELECT MIN(ts_min) FROM minute_agg;"), db);
    if (q.exec() && q.next() && !q.value(0).isNull()) {
        return q.value(0).toLongLong();
    }
    return 0;
}

qint64 HistoryStore::newestMs() const
{
    if (!m_open) return 0;
    auto db = QSqlDatabase::database(m_connection);
    QSqlQuery q(QStringLiteral("SELECT MAX(ts_min) FROM minute_agg;"), db);
    if (q.exec() && q.next() && !q.value(0).isNull()) {
        return q.value(0).toLongLong();
    }
    return 0;
}

bool HistoryStore::openConnection(bool *corrupt)
{
    if (corrupt) *corrupt = false;
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    db.setDatabaseName(m_path);
    if (!db.open()) {
        return fail(QStringLiteral("Cannot open sqlite database %1: %2").arg(m_path, db.lastError().text()));
    }

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA quick_check;"))) {
        if (corrupt) *corrupt = true;
        return fail(QStringLiteral("quick_check failed: %1").arg(q.lastError().text()));
    }
    if (q.next() && q.value(0).toString().toLower() != QStringLiteral("ok")) {
        if (corrupt) *corrupt = true;
        return fail(QStringLiteral("quick_check reported corruption: %1").arg(q.value(0).toString()));
    }

    q.exec(QStringLiteral("PRAGMA journal_mode=WAL;"));
    q.exec(QStringLiteral("PRAGMA synchronous=NORMAL;"));
    return true;
}

void HistoryStore::closeConnection()
{
    if (QSqlDatabase::contains(m_connection)) {
        {
            auto db = QSqlDatabase::database(m_connection, false);
            if (db.isOpen()) db.close();
        }
        QSqlDatabase::removeDatabase(m_connection);
    }
}

bool HistoryStore::setupSchema()
{
    auto db = QSqlDatabase::database(m_connection);
    const qint64 version = pragmaInt(QStringLiteral("user_version"));
    if (version > 1) return fail(QStringLiteral("History database schema is newer than this application supports."));
    const bool legacy = db.tables().contains(QStringLiteral("incidents")) && version == 0;
    if (legacy) {
        const QString backup = m_path + QStringLiteral(".pre-v1-backup-%1.sqlite").arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmsszzz")));
        QSqlQuery backupQuery(db);
        backupQuery.prepare(QStringLiteral("VACUUM INTO ?;"));
        backupQuery.addBindValue(backup);
        if (!backupQuery.exec()) return fail(QStringLiteral("Cannot back up history before migration: %1").arg(backupQuery.lastError().text()));
        m_recoveryNotice = QStringLiteral("History schema migrated; consistent backup saved to %1. Legacy incident calendar times remain unknown.").arg(backup);
    }
    if (!db.transaction()) return fail(db.lastError().text());
    QSqlQuery q(db);
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS minute_agg ("
                       "ts_min INTEGER NOT NULL, "
                       "metric TEXT NOT NULL, "
                       "min REAL NOT NULL, "
                       "avg REAL NOT NULL, "
                       "max REAL NOT NULL, "
                       "n INTEGER NOT NULL, "
                       "PRIMARY KEY(ts_min, metric)) WITHOUT ROWID;"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS incidents ("
                       "id TEXT PRIMARY KEY, "
                       "start_ms INTEGER NOT NULL, "
                       "end_ms INTEGER NOT NULL, "
                       "severity INTEGER NOT NULL, "
                       "rule_id TEXT, "
                       "headline TEXT, "
                       "domain TEXT, calendar_time_known INTEGER NOT NULL DEFAULT 1);"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS gaps ("
                       "start_ms INTEGER NOT NULL, "
                       "end_ms INTEGER NOT NULL, "
                       "reason TEXT NOT NULL, "
                       "PRIMARY KEY(start_ms, end_ms));"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_minute_metric ON minute_agg(metric, ts_min);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_incidents_range ON incidents(start_ms, end_ms);")
    };

    for (const auto &stmt : statements) {
        if (!q.exec(stmt)) {
            db.rollback();
            return fail(QStringLiteral("Failed to execute schema statement: %1 (%2)").arg(stmt, q.lastError().text()));
        }
    }
    if (legacy && (!q.exec(QStringLiteral("ALTER TABLE incidents ADD COLUMN calendar_time_known INTEGER NOT NULL DEFAULT 0;")))) {
        db.rollback(); return fail(q.lastError().text());
    }
    if (!q.exec(QStringLiteral("PRAGMA user_version=1;")) || !db.commit()) {
        db.rollback(); return fail(QStringLiteral("History schema migration failed."));
    }
    return true;
}

qint64 HistoryStore::legacyIncidentCount() const
{
    if (!m_open) return 0;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM incidents WHERE calendar_time_known = 0;")) || !q.next()) return 0;
    return q.value(0).toLongLong();
}

QString HistoryStore::connectionName() const
{
    return m_connection;
}

bool HistoryStore::fail(const QString &message)
{
    m_lastError = message;
    return false;
}

} // namespace LoofiPulse
