#include "flightrecorder.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <utility>

namespace LoofiPulse
{

FlightRecorder::FlightRecorder(int capacity)
    : m_capacity(std::max(10, capacity))
{
    m_samples.reserve(m_capacity);
}

quint64 FlightRecorder::appendSample(FlightSample sample)
{
    FlightSample stored = std::move(sample);
    if (stored.sampleId == 0) {
        stored.sampleId = m_nextSampleId++;
    } else {
        m_nextSampleId = std::max(m_nextSampleId, stored.sampleId + 1);
    }
    m_samples.append(stored);

    const qint64 latestTime = stored.monotonicMs > 0 ? stored.monotonicMs : stored.timestampMs;
    const qint64 cutoff = latestTime - m_retentionMs;
    while (!m_samples.isEmpty()) {
        const FlightSample &oldest = m_samples.first();
        const qint64 oldestTime = oldest.monotonicMs > 0 ? oldest.monotonicMs : oldest.timestampMs;
        if (m_samples.size() <= 1 || oldestTime >= cutoff) break;
        m_samples.removeFirst();
    }
    while (m_samples.size() > m_capacity) {
        m_samples.removeFirst();
    }
    return stored.sampleId;
}

void FlightRecorder::clear()
{
    m_samples.clear();
}

int FlightRecorder::count() const
{
    return static_cast<int>(m_samples.size());
}

int FlightRecorder::capacity() const
{
    return m_capacity;
}

void FlightRecorder::setCapacity(int capacity)
{
    m_capacity = std::max(10, capacity);
    while (m_samples.size() > m_capacity) m_samples.removeFirst();
    m_samples.reserve(m_capacity);
}

void FlightRecorder::setRetentionMs(qint64 retentionMs)
{
    m_retentionMs = std::clamp<qint64>(retentionMs, 1000, 60 * 60 * 1000);
    if (m_samples.size() > 1) {
        const FlightSample &latest = m_samples.last();
        const qint64 latestTime = latest.monotonicMs > 0 ? latest.monotonicMs : latest.timestampMs;
        const qint64 cutoff = latestTime - m_retentionMs;
        while (m_samples.size() > 1) {
            const FlightSample &oldest = m_samples.first();
            const qint64 oldestTime = oldest.monotonicMs > 0 ? oldest.monotonicMs : oldest.timestampMs;
            if (oldestTime >= cutoff) {
                break;
            }
            m_samples.removeFirst();
        }
    }
}

qint64 FlightRecorder::retentionMs() const
{
    return m_retentionMs;
}

int FlightRecorder::indexOfSample(quint64 sampleId) const
{
    for (int i = 0; i < m_samples.size(); ++i) {
        if (m_samples.at(i).sampleId == sampleId) return i;
    }
    return -1;
}

const QList<FlightSample> &FlightRecorder::samples() const
{
    return m_samples;
}

FlightSample FlightRecorder::sampleAt(int index) const
{
    if (index < 0 || index >= m_samples.size()) {
        return {};
    }
    return m_samples.at(index);
}

int FlightRecorder::peakTemperatureIndex() const
{
    int bestIdx = -1;
    double maxTemp = -1.0;
    for (int i = 0; i < m_samples.size(); ++i) {
        const FlightSample &sample = m_samples.at(i);
        if (sample.metricQuality.value(QStringLiteral("cpuTempAvailable")).toBool()
            && sample.cpuTempCelsius > maxTemp) {
            maxTemp = sample.cpuTempCelsius;
            bestIdx = i;
        }
    }
    return bestIdx;
}

int FlightRecorder::worstResponsivenessIndex() const
{
    int worstIdx = -1;
    double minScore = 100.0;
    for (int i = 0; i < m_samples.size(); ++i) {
        const FlightSample &sample = m_samples.at(i);
        if (sample.metricQuality.value(QStringLiteral("psiAvailable")).toBool()
            && sample.responsivenessScore < minScore) {
            minScore = sample.responsivenessScore;
            worstIdx = i;
        }
    }
    return worstIdx;
}

QVariantList FlightRecorder::incidentEpisodes() const
{
    QVariantList episodes;
    QVariantMap current;
    qint64 startMs = 0;
    int peakRank = -1;
    const auto finish = [&]() {
        if (!current.isEmpty()) episodes.append(current);
        current.clear();
        peakRank = -1;
    };
    for (const FlightSample &sample : m_samples) {
        // No incident duration or causality is inferred across unrecorded time.
        if (sample.metricQuality.value(QStringLiteral("gapBefore")).toBool()) finish();
        const bool warning = sample.severity == VerdictSeverity::Elevated
            || sample.severity == VerdictSeverity::Bottleneck
            || sample.severity == VerdictSeverity::CriticalThrottle;
        if ((!warning && !sample.throttled) || sample.severity == VerdictSeverity::Unknown) {
            finish();
            continue;
        }
        const qint64 timeMs = sample.monotonicMs > 0 ? sample.monotonicMs : sample.timestampMs;
        if (current.isEmpty()) startMs = timeMs;
        const int rank = sample.throttled ? 4 : static_cast<int>(sample.severity);
        if (rank > peakRank) {
            current.insert(QStringLiteral("sampleId"), static_cast<qulonglong>(sample.sampleId));
            current.insert(QStringLiteral("timeString"), QDateTime::fromMSecsSinceEpoch(sample.timestampMs).toString(QStringLiteral("HH:mm:ss")));
            current.insert(QStringLiteral("headline"), sample.verdictHeadline);
            current.insert(QStringLiteral("severity"), static_cast<int>(sample.severity));
            current.insert(QStringLiteral("throttled"), sample.throttled);
            peakRank = rank;
        }
        current.insert(QStringLiteral("durationSeconds"), std::max<qint64>(1, (timeMs - startMs) / 1000));
        current.insert(QStringLiteral("endTimeString"), QDateTime::fromMSecsSinceEpoch(sample.timestampMs).toString(QStringLiteral("HH:mm:ss")));
    }
    finish();
    while (episodes.size() > 8) episodes.removeFirst();
    return episodes;
}

QString FlightRecorder::exportSummaryMarkdown(bool includeProcessDetails) const
{
    if (m_samples.isEmpty()) {
        return QStringLiteral("# Loofi Pulse — Flight Recorder Report\n\nNo telemetry samples recorded.");
    }

    const FlightSample &latest = m_samples.last();
    const int peakIdx = peakTemperatureIndex();
    const int worstIdx = worstResponsivenessIndex();

    qint64 throttleEvents = 0;
    bool throttleCountersAvailable = false;
    for (const FlightSample &s : m_samples) {
        if (s.metricQuality.value(QStringLiteral("throttleCountersAvailable")).toBool()) {
            throttleCountersAvailable = true;
            throttleEvents += s.newThrottleEvents;
        }
    }

    QString md;
    md += QStringLiteral("# Loofi Pulse — Hardware History Report\n\n");
    const qint64 firstMs = m_samples.first().timestampMs;
    const qint64 lastMs = m_samples.last().timestampMs;
    const qint64 durationSeconds = firstMs > 0 && lastMs >= firstMs ? (lastMs - firstMs) / 1000 : 0;
    md += QStringLiteral("- **Recorded Samples**: %1\n").arg(m_samples.size());
    md += QStringLiteral("- **Covered Period**: %1 seconds\n").arg(durationSeconds);
    md += QStringLiteral("- **Throttle Events Observed**: %1\n")
              .arg(throttleCountersAvailable ? QString::number(throttleEvents) : QStringLiteral("Unavailable"));
    md += QStringLiteral("- **Latest Verdict**: %1\n").arg(latest.verdictHeadline);
    const auto metricText = [](const FlightSample &sample, const QString &qualityKey, double value, int precision, const QString &unit) {
        return sample.metricQuality.value(qualityKey).toBool()
            ? QStringLiteral("%1%2").arg(value, 0, 'f', precision).arg(unit)
            : QStringLiteral("Unavailable");
    };
    md += QStringLiteral("- **Latest CPU / Power**: temperature %1; load %2; frequency %3; CPU package power %4\n\n")
              .arg(metricText(latest, QStringLiteral("cpuTempAvailable"), latest.cpuTempCelsius, 1, QStringLiteral("°C")),
                   metricText(latest, QStringLiteral("cpuLoadAvailable"), latest.cpuLoadPercent, 1, QStringLiteral("%")),
                   latest.metricQuality.value(QStringLiteral("cpuFrequencyAvailable")).toBool()
                       ? QStringLiteral("%1 MHz").arg(latest.cpuFreqMhz) : QStringLiteral("Unavailable"),
                   metricText(latest, QStringLiteral("packagePowerAvailable"), latest.packageWatts, 1, QStringLiteral(" W")));

    md += QStringLiteral("- **Battery discharge**: %1\n\n")
              .arg(metricText(latest, QStringLiteral("batteryDischargeAvailable"), latest.batteryWatts, 1, QStringLiteral(" W")));
    const QVariantMap metadata = sampleMetricMetadata(latest);
    if (!metadata.isEmpty()) {
        md += QStringLiteral("## Data quality at latest sample\n");
        for (const QString &key : {QStringLiteral("cpuLoad"), QStringLiteral("cpuTemperature"),
                                   QStringLiteral("pressureIndex"), QStringLiteral("packagePower")}) {
            const QVariantMap quality = metadata.value(key).toMap();
            if (quality.isEmpty()) continue;
            const qint64 sampledAtMs = quality.value(QStringLiteral("sampledAtMs")).toLongLong();
            const QString time = sampledAtMs > 0
                ? QDateTime::fromMSecsSinceEpoch(sampledAtMs).toString(Qt::ISODate)
                : QStringLiteral("Unavailable");
            md += QStringLiteral("- **%1**: %2; source: %3; sampled: %4")
                      .arg(key, quality.value(QStringLiteral("state")).toString(),
                           quality.value(QStringLiteral("source")).toString(), time);
            const QString detail = quality.value(QStringLiteral("detail")).toString();
            if (!detail.isEmpty()) md += QStringLiteral("; %1").arg(detail);
            md += QLatin1Char('\n');
        }
        md += QLatin1Char('\n');
    }

    if (peakIdx >= 0) {
        const FlightSample &p = m_samples.at(peakIdx);
        const QString tStr = QDateTime::fromMSecsSinceEpoch(p.timestampMs).toString(QStringLiteral("HH:mm:ss"));
        md += QStringLiteral("## Peak Thermal Spike (%1)\n").arg(tStr);
        md += QStringLiteral("- **CPU Package Temp**: %1\n")
                  .arg(metricText(p, QStringLiteral("cpuTempAvailable"), p.cpuTempCelsius, 1, QStringLiteral("°C")));
        md += QStringLiteral("- **Concurrent load observation**: %1\n\n")
                  .arg((!includeProcessDetails || p.culpritName.isEmpty())
                       ? QStringLiteral("Unavailable; process activity is not evidence of causation.")
                       : QStringLiteral("%1 (PID %2, %3% CPU, %4 MiB RSS); concurrent activity, not a confirmed cause.")
                             .arg(p.culpritName).arg(p.culpritPid)
                             .arg(p.culpritCpuPercent, 0, 'f', 1).arg(p.culpritRssMb, 0, 'f', 0));
    }

    if (worstIdx >= 0) {
        const FlightSample &w = m_samples.at(worstIdx);
        const QString tStr = QDateTime::fromMSecsSinceEpoch(w.timestampMs).toString(QStringLiteral("HH:mm:ss"));
        md += QStringLiteral("## Worst PSI Stall / Responsiveness Drop (%1)\n").arg(tStr);
        md += QStringLiteral("- **Responsiveness index (higher is better)**: %1\n")
                  .arg(metricText(w, QStringLiteral("psiAvailable"), w.responsivenessScore, 1, QStringLiteral("/100")));
        md += QStringLiteral("- **PSI averages**: CPU some %1; memory some %2; I/O full %3\n")
                  .arg(metricText(w, QStringLiteral("psiCpuSomeAvailable"), w.psiCpuSome10, 2, QStringLiteral("%")),
                       metricText(w, QStringLiteral("psiMemorySomeAvailable"), w.psiMemSome10, 2, QStringLiteral("%")),
                       metricText(w, QStringLiteral("psiIoFullAvailable"), w.psiIoFull10, 2, QStringLiteral("%")));
        md += QStringLiteral("- **Concurrent load observation**: %1; this does not establish a cause.\n")
                  .arg((!includeProcessDetails || w.culpritName.isEmpty()) ? QStringLiteral("Unavailable") : w.culpritName);
    } else if (latest.metricQuality.value(QStringLiteral("psiAvailable")).toBool()) {
        md += QStringLiteral("No responsiveness-index drop below 100/100 was recorded.\n");
    } else {
        md += QStringLiteral("Responsiveness index: Unavailable; required PSI readings were incomplete.\n");
    }

    return md;
}

QVariantMap FlightRecorder::sampleMetricMetadata(const FlightSample &s)
{
    const QVariant explicitMeta = s.metricQuality.value(QStringLiteral("metricMetadata"));
    if (explicitMeta.isValid() && !explicitMeta.toMap().isEmpty()) {
        return explicitMeta.toMap();
    }
    if (s.metricQuality.isEmpty()) {
        return {};
    }

    const qint64 sampledAt = s.timestampMs;
    const auto flag = [&s](const QString &key) { return s.metricQuality.value(key).toBool(); };
    const auto metric = [sampledAt](const QString &state, const QString &source, const QString &detail = QString()) {
        return QVariantMap{{QStringLiteral("state"), state},
                           {QStringLiteral("source"), source},
                           {QStringLiteral("detail"), detail},
                           {QStringLiteral("sampledAtMs"), sampledAt}};
    };

    QVariantMap result;
    const bool loadAvail = flag(QStringLiteral("cpuLoadAvailable"));
    const bool loadWarm = flag(QStringLiteral("cpuLoadWarmedUp"));
    result.insert(QStringLiteral("cpuLoad"),
                  metric(loadAvail ? QStringLiteral("available") : (loadWarm ? QStringLiteral("warming-up") : QStringLiteral("unavailable")),
                         QStringLiteral("/proc/stat"),
                         loadAvail ? QString() : QStringLiteral("CPU counters could not be read.")));
    result.insert(QStringLiteral("cpuTemperature"),
                  metric(flag(QStringLiteral("cpuTempAvailable")) ? QStringLiteral("available") : QStringLiteral("unavailable"),
                         QStringLiteral("/sys/class/hwmon temp*_input"),
                         flag(QStringLiteral("cpuTempAvailable")) ? QString() : QStringLiteral("No CPU temperature sensor was found.")));
    result.insert(QStringLiteral("thermalHeadroom"),
                  metric(flag(QStringLiteral("thermalHeadroomAvailable")) ? QStringLiteral("available") : QStringLiteral("unavailable"),
                         QStringLiteral("/sys/class/hwmon temp*_max or temp*_crit")));
    for (const auto &entry : {qMakePair(QStringLiteral("cpuPressure"), QStringLiteral("psiCpuSomeAvailable")),
                              qMakePair(QStringLiteral("memoryPressure"), QStringLiteral("psiMemorySomeAvailable")),
                              qMakePair(QStringLiteral("ioPressure"), QStringLiteral("psiIoSomeAvailable"))}) {
        result.insert(entry.first, metric(flag(entry.second) ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                         QStringLiteral("/proc/pressure/") + entry.first.left(entry.first.indexOf(QStringLiteral("Pressure")))));
    }
    result.insert(QStringLiteral("memoryUsage"), metric(flag(QStringLiteral("ramAvailable")) ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                        QStringLiteral("/proc/meminfo MemTotal and MemAvailable")));
    result.insert(QStringLiteral("zram"), metric(flag(QStringLiteral("zramAvailable")) ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                QStringLiteral("/sys/block/zram*/mm_stat")));
    result.insert(QStringLiteral("battery"), metric(flag(QStringLiteral("batteryAvailable")) ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                   QStringLiteral("/sys/class/power_supply")));
    result.insert(QStringLiteral("batteryDischarge"), metric(flag(QStringLiteral("batteryDischargeAvailable")) ? QStringLiteral("available") : QStringLiteral("unavailable"),
        QStringLiteral("/sys/class/power_supply battery power"), QStringLiteral("Battery discharge is separate from CPU package power.")));
    result.insert(QStringLiteral("batteryPower"), metric(flag(QStringLiteral("batteryPowerAvailable")) ? QStringLiteral("available") : QStringLiteral("unavailable"),
                                                        QStringLiteral("/sys/class/power_supply power_now or current_now × voltage_now")));
    result.insert(QStringLiteral("processActivity"), QVariantMap{
        {QStringLiteral("state"), s.processObservedAtMs > 0 ? QStringLiteral("available") : QStringLiteral("unavailable")},
        {QStringLiteral("source"), QStringLiteral("/proc/PID/stat; concurrent observation, not a confirmed cause")},
        {QStringLiteral("sampledAtMs"), s.processObservedAtMs}});
    result.insert(QStringLiteral("pressureIndex"),
                  metric(flag(QStringLiteral("psiAvailable")) ? QStringLiteral("calculated") : QStringLiteral("unavailable"),
                         QStringLiteral("Calculated from PSI avg10 values"),
                         QStringLiteral("This index is not a measured response time.")));
    const bool pkgPowerAvail = flag(QStringLiteral("packagePowerAvailable"));
    const bool pkgPowerEst = flag(QStringLiteral("packagePowerEstimated"));
    result.insert(QStringLiteral("packagePower"),
                  metric(!pkgPowerAvail ? QStringLiteral("unavailable") : (pkgPowerEst ? QStringLiteral("estimated") : QStringLiteral("available")),
                         pkgPowerEst ? QStringLiteral("Estimated from CPU load and frequency") : QStringLiteral("Hardware power sensor")));
    result.insert(QStringLiteral("gpuLoad"),
                  metric(flag(QStringLiteral("gpuBusyAvailable")) ? QStringLiteral("available") : QStringLiteral("unavailable"),
                         QStringLiteral("/sys/class/drm device/gpu_busy_percent")));
    result.insert(QStringLiteral("gpuTemperature"),
                  metric(flag(QStringLiteral("gpuTempAvailable")) ? QStringLiteral("available") : QStringLiteral("unavailable"),
                         QStringLiteral("GPU device hwmon temp*_input")));
    result.insert(QStringLiteral("diskThroughput"),
                  metric(flag(QStringLiteral("diskThroughputAvailable")) ? QStringLiteral("available") : QStringLiteral("unavailable"),
                         QStringLiteral("/proc/diskstats sector deltas")));
    result.insert(QStringLiteral("netThroughput"),
                  metric(flag(QStringLiteral("netThroughputAvailable")) ? QStringLiteral("available") : QStringLiteral("unavailable"),
                         QStringLiteral("/proc/net/dev byte deltas")));
    return result;
}

QString FlightRecorder::exportSamplesJson(bool includeProcessDetails) const
{
    QJsonArray arr;
    const qint64 recordedFromMs = m_samples.isEmpty() ? 0 : m_samples.first().timestampMs;
    const qint64 recordedToMs = m_samples.isEmpty() ? 0 : m_samples.last().timestampMs;
    const qint64 recordedPeriodMs = recordedFromMs > 0 && recordedToMs >= recordedFromMs
        ? recordedToMs - recordedFromMs : 0;
    for (int i = 0; i < m_samples.size(); ++i) {
        const FlightSample &s = m_samples.at(i);
        const auto valid = [&s](const QString &key) { return s.metricQuality.value(key).toBool(); };
        const auto valueOrNull = [&valid](const QString &qualityKey, double value) {
            return valid(qualityKey) ? QJsonValue(value) : QJsonValue(QJsonValue::Null);
        };
        QJsonObject obj;
        obj.insert(QStringLiteral("index"), i);
        obj.insert(QStringLiteral("sampleId"), static_cast<qint64>(s.sampleId));
        obj.insert(QStringLiteral("timestampMs"), s.timestampMs);
        obj.insert(QStringLiteral("monotonicMs"), s.monotonicMs);
        obj.insert(QStringLiteral("elapsedSincePreviousMs"), s.elapsedSincePreviousMs);
        obj.insert(QStringLiteral("recordedFromMs"), recordedFromMs);
        obj.insert(QStringLiteral("recordedToMs"), recordedToMs);
        obj.insert(QStringLiteral("recordedPeriodMs"), recordedPeriodMs);
        obj.insert(QStringLiteral("cpuTemp"), valueOrNull(QStringLiteral("cpuTempAvailable"), s.cpuTempCelsius));
        obj.insert(QStringLiteral("cpuTempCelsius"), valueOrNull(QStringLiteral("cpuTempAvailable"), s.cpuTempCelsius));
        obj.insert(QStringLiteral("cpuLoadPercent"), valueOrNull(QStringLiteral("cpuLoadAvailable"), s.cpuLoadPercent));
        obj.insert(QStringLiteral("cpuFreqMhz"), valid(QStringLiteral("cpuFrequencyAvailable")) ? QJsonValue(s.cpuFreqMhz) : QJsonValue(QJsonValue::Null));
        obj.insert(QStringLiteral("gpuBusyPercent"), valueOrNull(QStringLiteral("gpuBusyAvailable"), s.gpuBusyPercent));
        obj.insert(QStringLiteral("gpuTempCelsius"), valueOrNull(QStringLiteral("gpuTempAvailable"), s.gpuTempCelsius));
        obj.insert(QStringLiteral("nvmeTempCelsius"), valueOrNull(QStringLiteral("nvmeTempAvailable"), s.nvmeTempCelsius));
        obj.insert(QStringLiteral("wifiTempCelsius"), valueOrNull(QStringLiteral("wifiTempAvailable"), s.wifiTempCelsius));
        obj.insert(QStringLiteral("gpuFreqMhz"), valid(QStringLiteral("gpuFrequencyAvailable")) ? QJsonValue(s.gpuFreqMhz) : QJsonValue(QJsonValue::Null));
        obj.insert(QStringLiteral("gpuPowerWatts"), valueOrNull(QStringLiteral("gpuPowerAvailable"), s.gpuPowerWatts));
        obj.insert(QStringLiteral("gpuVramUsedMegabytes"), valueOrNull(QStringLiteral("gpuVramAvailable"), s.gpuVramUsedMegabytes));
        obj.insert(QStringLiteral("gpuVramTotalMegabytes"), valueOrNull(QStringLiteral("gpuVramAvailable"), s.gpuVramTotalMegabytes));
        obj.insert(QStringLiteral("psiCpuSome10"), valueOrNull(QStringLiteral("psiCpuSomeAvailable"), s.psiCpuSome10));
        obj.insert(QStringLiteral("psiCpuSome60"), valueOrNull(QStringLiteral("psiCpuSomeAvailable"), s.psiCpuSome60));
        obj.insert(QStringLiteral("psiMemSome10"), valueOrNull(QStringLiteral("psiMemorySomeAvailable"), s.psiMemSome10));
        obj.insert(QStringLiteral("psiMemSome60"), valueOrNull(QStringLiteral("psiMemorySomeAvailable"), s.psiMemSome60));
        obj.insert(QStringLiteral("psiMemFull10"), valueOrNull(QStringLiteral("psiMemoryFullAvailable"), s.psiMemFull10));
        obj.insert(QStringLiteral("psiMemFull60"), valueOrNull(QStringLiteral("psiMemoryFullAvailable"), s.psiMemFull60));
        obj.insert(QStringLiteral("psiIoSome10"), valueOrNull(QStringLiteral("psiIoSomeAvailable"), s.psiIoSome10));
        obj.insert(QStringLiteral("psiIoSome60"), valueOrNull(QStringLiteral("psiIoSomeAvailable"), s.psiIoSome60));
        obj.insert(QStringLiteral("psiIoFull10"), valueOrNull(QStringLiteral("psiIoFullAvailable"), s.psiIoFull10));
        obj.insert(QStringLiteral("psiIoFull60"), valueOrNull(QStringLiteral("psiIoFullAvailable"), s.psiIoFull60));
        obj.insert(QStringLiteral("psiCpuStallMsPerSec"), valueOrNull(QStringLiteral("psiCpuSomeAvailable"), s.psiCpuStallMsPerSec));
        obj.insert(QStringLiteral("psiMemStallMsPerSec"), valueOrNull(QStringLiteral("psiMemorySomeAvailable"), s.psiMemStallMsPerSec));
        obj.insert(QStringLiteral("psiIoStallMsPerSec"), valueOrNull(QStringLiteral("psiIoSomeAvailable"), s.psiIoStallMsPerSec));
        QVariantMap exportedQuality = s.metricQuality;
        if (!includeProcessDetails) {
            QVariantMap nestedMetadata = exportedQuality.value(QStringLiteral("metricMetadata")).toMap();
            nestedMetadata.remove(QStringLiteral("processActivity"));
            if (exportedQuality.contains(QStringLiteral("metricMetadata"))) exportedQuality.insert(QStringLiteral("metricMetadata"), nestedMetadata);
        }
        obj.insert(QStringLiteral("metricQuality"), QJsonObject::fromVariantMap(exportedQuality));
        QVariantMap metadata = sampleMetricMetadata(s);
        if (!includeProcessDetails) metadata.remove(QStringLiteral("processActivity"));
        obj.insert(QStringLiteral("metricMetadata"), QJsonObject::fromVariantMap(metadata));
        obj.insert(QStringLiteral("responsivenessScore"), valueOrNull(QStringLiteral("psiAvailable"), s.responsivenessScore));
        obj.insert(QStringLiteral("responseIndexAvailable"), valid(QStringLiteral("psiAvailable")));
        obj.insert(QStringLiteral("systemWatts"), valueOrNull(QStringLiteral("systemPowerAvailable"), s.systemWatts));
        obj.insert(QStringLiteral("systemPowerEstimated"), s.metricQuality.value(QStringLiteral("systemPowerEstimated")).toBool());
        obj.insert(QStringLiteral("packageWatts"), valueOrNull(QStringLiteral("packagePowerAvailable"), s.packageWatts));
        obj.insert(QStringLiteral("batteryWatts"), valueOrNull(QStringLiteral("batteryPowerAvailable"), s.batteryWatts));
        obj.insert(QStringLiteral("batteryDischargeWatts"), valueOrNull(QStringLiteral("batteryDischargeAvailable"), s.batteryWatts));
        obj.insert(QStringLiteral("batteryPercent"), valueOrNull(QStringLiteral("batteryLevelAvailable"), s.batteryPercent));
        obj.insert(QStringLiteral("batteryHealthPercent"), valueOrNull(QStringLiteral("batteryHealthAvailable"), s.batteryHealthPercent));
        obj.insert(QStringLiteral("pl1LimitWatts"), valueOrNull(QStringLiteral("powerLimitsAvailable"), s.pl1LimitWatts));
        obj.insert(QStringLiteral("pl2LimitWatts"), valueOrNull(QStringLiteral("powerLimitsAvailable"), s.pl2LimitWatts));
        obj.insert(QStringLiteral("fanRpm"), valid(QStringLiteral("fanAvailable")) ? QJsonValue(s.fanRpm) : QJsonValue(QJsonValue::Null));
        obj.insert(QStringLiteral("ramUsedPercent"), valueOrNull(QStringLiteral("ramAvailable"), s.ramUsedPercent));
        obj.insert(QStringLiteral("ramUsedBytes"), valueOrNull(QStringLiteral("ramAvailable"), static_cast<double>(s.ramUsedBytes)));
        obj.insert(QStringLiteral("ramTotalBytes"), valueOrNull(QStringLiteral("ramAvailable"), static_cast<double>(s.ramTotalBytes)));
        obj.insert(QStringLiteral("swapUsedBytes"), valueOrNull(QStringLiteral("swapAvailable"), static_cast<double>(s.swapUsedBytes)));
        obj.insert(QStringLiteral("swapTotalBytes"), valueOrNull(QStringLiteral("swapAvailable"), static_cast<double>(s.swapTotalBytes)));
        obj.insert(QStringLiteral("zramCompressionRatio"), valueOrNull(QStringLiteral("zramCompressionAvailable"), s.zramCompressionRatio));
        obj.insert(QStringLiteral("zramOriginalBytes"), valueOrNull(QStringLiteral("zramAvailable"), static_cast<double>(s.zramOriginalBytes)));
        obj.insert(QStringLiteral("zramCompressedBytes"), valueOrNull(QStringLiteral("zramAvailable"), static_cast<double>(s.zramCompressedBytes)));
        obj.insert(QStringLiteral("zramMemoryUsedBytes"), valueOrNull(QStringLiteral("zramAvailable"), static_cast<double>(s.zramMemoryUsedBytes)));
        obj.insert(QStringLiteral("diskReadMibPerSec"), valueOrNull(QStringLiteral("diskThroughputAvailable"), s.diskReadMibPerSec));
        obj.insert(QStringLiteral("diskWriteMibPerSec"), valueOrNull(QStringLiteral("diskThroughputAvailable"), s.diskWriteMibPerSec));
        obj.insert(QStringLiteral("netRxMibPerSec"), valueOrNull(QStringLiteral("netThroughputAvailable"), s.netRxMibPerSec));
        obj.insert(QStringLiteral("netTxMibPerSec"), valueOrNull(QStringLiteral("netThroughputAvailable"), s.netTxMibPerSec));
        obj.insert(QStringLiteral("throttleEventCount"), valid(QStringLiteral("throttleCountersAvailable")) ? QJsonValue(s.throttleEventCount) : QJsonValue(QJsonValue::Null));
        obj.insert(QStringLiteral("newThrottleEvents"), valid(QStringLiteral("throttleCountersAvailable")) ? QJsonValue(s.newThrottleEvents) : QJsonValue(QJsonValue::Null));
        obj.insert(QStringLiteral("batteryPresent"), s.batteryPresent);
        obj.insert(QStringLiteral("onAcPower"), valid(QStringLiteral("powerStateAvailable")) ? QJsonValue(s.onAcPower) : QJsonValue(QJsonValue::Null));
        obj.insert(QStringLiteral("activePowerProfile"), s.activePowerProfile.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(s.activePowerProfile));
        obj.insert(QStringLiteral("throttled"), s.throttled);
        obj.insert(QStringLiteral("severity"), static_cast<int>(s.severity));
        obj.insert(QStringLiteral("verdictHeadline"), s.verdictHeadline);
        obj.insert(QStringLiteral("culpritName"), includeProcessDetails ? s.culpritName : QString());
        obj.insert(QStringLiteral("culpritPid"), includeProcessDetails ? s.culpritPid : 0);
        obj.insert(QStringLiteral("culpritStartTimeTicks"), includeProcessDetails ? QJsonValue(static_cast<qint64>(s.culpritStartTimeTicks)) : QJsonValue(QJsonValue::Null));
        obj.insert(QStringLiteral("processObservedAtMs"), includeProcessDetails ? QJsonValue(s.processObservedAtMs) : QJsonValue(QJsonValue::Null));
        obj.insert(QStringLiteral("culpritCpuPercent"), includeProcessDetails ? s.culpritCpuPercent : 0.0);
        obj.insert(QStringLiteral("culpritRssMb"), includeProcessDetails ? s.culpritRssMb : 0.0);
        obj.insert(QStringLiteral("schemaVersion"), 2);
        obj.insert(QStringLiteral("processDetailsIncluded"), includeProcessDetails);
        obj.insert(QStringLiteral("verdictRuleId"), s.verdictRuleId);
        obj.insert(QStringLiteral("verdictEvidence"), QJsonArray::fromStringList(includeProcessDetails ? s.verdictEvidence : QStringList()));
        obj.insert(QStringLiteral("verdictDurationMs"), s.verdictDurationMs);
        obj.insert(QStringLiteral("powerSources"), QJsonObject{
            {QStringLiteral("packageWatts"), QStringLiteral("CPU package only; see metricMetadata.packagePower")},
            {QStringLiteral("batteryWatts"), QStringLiteral("Battery discharge or charge power")},
            {QStringLiteral("systemWatts"), QStringLiteral("Legacy composite field; do not interpret as measured whole-system power")}});
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

} // namespace LoofiPulse
