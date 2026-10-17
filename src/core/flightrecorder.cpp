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

void FlightRecorder::setRetentionMs(qint64 retentionMs)
{
    m_retentionMs = std::clamp<qint64>(retentionMs, 1000, 60 * 60 * 1000);
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

QString FlightRecorder::exportSummaryMarkdown() const
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
    md += QStringLiteral("- **Latest CPU / Power**: temperature %1; load %2; frequency %3; system power %4\n\n")
              .arg(metricText(latest, QStringLiteral("cpuTempAvailable"), latest.cpuTempCelsius, 1, QStringLiteral("°C")),
                   metricText(latest, QStringLiteral("cpuLoadAvailable"), latest.cpuLoadPercent, 1, QStringLiteral("%")),
                   latest.metricQuality.value(QStringLiteral("cpuFrequencyAvailable")).toBool()
                       ? QStringLiteral("%1 MHz").arg(latest.cpuFreqMhz) : QStringLiteral("Unavailable"),
                   metricText(latest, QStringLiteral("systemPowerAvailable"), latest.systemWatts, 1, QStringLiteral(" W")));

    const QVariantMap metadata = latest.metricQuality.value(QStringLiteral("metricMetadata")).toMap();
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
                  .arg(p.culpritName.isEmpty()
                       ? QStringLiteral("Unavailable; process activity is not evidence of causation.")
                       : QStringLiteral("%1 (PID %2, %3% CPU, %4 MiB RSS); concurrent activity, not a confirmed cause.")
                             .arg(p.culpritName).arg(p.culpritPid)
                             .arg(p.culpritCpuPercent, 0, 'f', 1).arg(p.culpritRssMb, 0, 'f', 0));
    }

    if (worstIdx >= 0) {
        const FlightSample &w = m_samples.at(worstIdx);
        const QString tStr = QDateTime::fromMSecsSinceEpoch(w.timestampMs).toString(QStringLiteral("HH:mm:ss"));
        md += QStringLiteral("## Worst PSI Stall / Responsiveness Drop (%1)\n").arg(tStr);
        md += QStringLiteral("- **Calculated pressure index**: %1\n")
                  .arg(metricText(w, QStringLiteral("psiAvailable"), w.responsivenessScore, 1, QStringLiteral("%")));
        md += QStringLiteral("- **PSI averages**: CPU some %1; memory some %2; I/O full %3\n")
                  .arg(metricText(w, QStringLiteral("psiCpuSomeAvailable"), w.psiCpuSome10, 2, QStringLiteral("%")),
                       metricText(w, QStringLiteral("psiMemorySomeAvailable"), w.psiMemSome10, 2, QStringLiteral("%")),
                       metricText(w, QStringLiteral("psiIoFullAvailable"), w.psiIoFull10, 2, QStringLiteral("%")));
        md += QStringLiteral("- **Concurrent load observation**: %1; this does not establish a cause.\n")
                  .arg(w.culpritName.isEmpty() ? QStringLiteral("Unavailable") : w.culpritName);
    } else if (latest.metricQuality.value(QStringLiteral("psiAvailable")).toBool()) {
        md += QStringLiteral("No pressure-index drop below 100% was recorded.\n");
    } else {
        md += QStringLiteral("Pressure index: Unavailable; required PSI readings were incomplete.\n");
    }

    return md;
}

QString FlightRecorder::exportSamplesJson() const
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
        obj.insert(QStringLiteral("metricQuality"), QJsonObject::fromVariantMap(s.metricQuality));
        obj.insert(QStringLiteral("metricMetadata"), QJsonObject::fromVariantMap(s.metricQuality.value(QStringLiteral("metricMetadata")).toMap()));
        obj.insert(QStringLiteral("responsivenessScore"), valueOrNull(QStringLiteral("psiAvailable"), s.responsivenessScore));
        obj.insert(QStringLiteral("responseIndexAvailable"), valid(QStringLiteral("psiAvailable")));
        obj.insert(QStringLiteral("systemWatts"), valueOrNull(QStringLiteral("systemPowerAvailable"), s.systemWatts));
        obj.insert(QStringLiteral("systemPowerEstimated"), s.metricQuality.value(QStringLiteral("systemPowerEstimated")).toBool());
        obj.insert(QStringLiteral("packageWatts"), valueOrNull(QStringLiteral("packagePowerAvailable"), s.packageWatts));
        obj.insert(QStringLiteral("batteryWatts"), valueOrNull(QStringLiteral("batteryPowerAvailable"), s.batteryWatts));
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
        obj.insert(QStringLiteral("throttleEventCount"), valid(QStringLiteral("throttleCountersAvailable")) ? QJsonValue(s.throttleEventCount) : QJsonValue(QJsonValue::Null));
        obj.insert(QStringLiteral("newThrottleEvents"), valid(QStringLiteral("throttleCountersAvailable")) ? QJsonValue(s.newThrottleEvents) : QJsonValue(QJsonValue::Null));
        obj.insert(QStringLiteral("batteryPresent"), s.batteryPresent);
        obj.insert(QStringLiteral("onAcPower"), valid(QStringLiteral("powerStateAvailable")) ? QJsonValue(s.onAcPower) : QJsonValue(QJsonValue::Null));
        obj.insert(QStringLiteral("activePowerProfile"), s.activePowerProfile.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(s.activePowerProfile));
        obj.insert(QStringLiteral("throttled"), s.throttled);
        obj.insert(QStringLiteral("severity"), static_cast<int>(s.severity));
        obj.insert(QStringLiteral("verdictHeadline"), s.verdictHeadline);
        obj.insert(QStringLiteral("culpritName"), s.culpritName);
        obj.insert(QStringLiteral("culpritPid"), s.culpritPid);
        obj.insert(QStringLiteral("culpritCpuPercent"), s.culpritCpuPercent);
        obj.insert(QStringLiteral("culpritRssMb"), s.culpritRssMb);
        obj.insert(QStringLiteral("schemaVersion"), 1);
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

} // namespace LoofiPulse
