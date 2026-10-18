#include "pulsetelemetrycontroller.h"

#include <QJsonDocument>

#include <algorithm>
#include <cmath>

namespace LoofiPulse {
namespace {
QVariantMap deviceWithId(const QVariantList &devices, const QString &id)
{
    for (const auto &value : devices) {
        const auto device = value.toMap();
        if (device.value(QStringLiteral("id")).toString() == id) return device;
    }
    return {};
}
QVariantMap gpuValues(const QVariantMap &device)
{
    return {{QStringLiteral("gpuAvailable"), device.value(QStringLiteral("available"), false)},
        {QStringLiteral("gpuDriverName"), device.value(QStringLiteral("driverName"), QString())},
        {QStringLiteral("gpuCurrentFreqMhz"), device.value(QStringLiteral("currentFreqMhz"), 0)},
        {QStringLiteral("gpuBoostFreqMhz"), device.value(QStringLiteral("boostFreqMhz"), 0)},
        {QStringLiteral("gpuFrequencyAvailable"), device.value(QStringLiteral("frequencyAvailable"), false)},
        {QStringLiteral("gpuBusyPercent"), device.value(QStringLiteral("busyPercent"), 0.0)},
        {QStringLiteral("gpuBusyAvailable"), device.value(QStringLiteral("busyAvailable"), false)},
        {QStringLiteral("gpuTempCelsius"), device.value(QStringLiteral("tempCelsius"), 0.0)},
        {QStringLiteral("gpuTempAvailable"), device.value(QStringLiteral("temperatureAvailable"), false)},
        {QStringLiteral("gpuPowerWatts"), device.value(QStringLiteral("powerWatts"), 0.0)},
        {QStringLiteral("gpuPowerAvailable"), device.value(QStringLiteral("powerAvailable"), false)},
        {QStringLiteral("gpuVramUsedMib"), device.value(QStringLiteral("vramUsedMegabytes"), 0.0)},
        {QStringLiteral("gpuVramTotalMib"), device.value(QStringLiteral("vramTotalMegabytes"), 0.0)},
        {QStringLiteral("gpuVramAvailable"), device.value(QStringLiteral("vramAvailable"), false)}};
}
QVariantMap diskValues(const QVariantMap &device)
{
    return {{QStringLiteral("diskReadMibPerSec"), device.value(QStringLiteral("readMibPerSec"), 0.0)},
        {QStringLiteral("diskWriteMibPerSec"), device.value(QStringLiteral("writeMibPerSec"), 0.0)},
        {QStringLiteral("diskThroughputAvailable"), device.value(QStringLiteral("available"), false)}};
}
QVariantMap networkValues(const QVariantList &devices, const QStringList &selected)
{
    double received = 0, sent = 0;
    bool available = false;
    for (const auto &value : devices) {
        const auto device = value.toMap();
        const bool included = selected.isEmpty() ? device.value(QStringLiteral("physical")).toBool()
            : selected.contains(device.value(QStringLiteral("name")).toString());
        if (!included || !device.value(QStringLiteral("available")).toBool()) continue;
        available = true;
        received += device.value(QStringLiteral("rxMibPerSec")).toDouble();
        sent += device.value(QStringLiteral("txMibPerSec")).toDouble();
    }
    return {{QStringLiteral("netRxMibPerSec"), received}, {QStringLiteral("netTxMibPerSec"), sent},
        {QStringLiteral("netThroughputAvailable"), available}};
}
void updateMetadata(QVariantMap &metadata, const QString &key, bool available, const QString &identity)
{
    auto item = metadata.value(key).toMap();
    item[QStringLiteral("state")] = available ? QStringLiteral("available") : QStringLiteral("unavailable");
    item[QStringLiteral("deviceIdentity")] = identity;
    item[QStringLiteral("detail")] = available ? QString() : QStringLiteral("The selected device has no available measurement for this observation.");
    metadata[key] = item;
}
}

QVariantList PulseTelemetryController::remoteCollection(const char *key) const
{
    auto rows = m_remoteValues.value(QString::fromLatin1(key)).toList();
    if (m_receivedAt.isValid() && !m_connectionState.startsWith(QStringLiteral("Disconnected"))
        && m_receivedAt.elapsed() <= std::max(5000, m_timer.interval() * 3)) return rows;
    for (auto &value : rows) {
        auto row = value.toMap();
        for (auto it = row.begin(); it != row.end(); ++it)
            if (it.key() == QStringLiteral("available") || it.key().endsWith(QStringLiteral("Available"))) it.value() = false;
        row[QStringLiteral("detail")] = QStringLiteral("No current observation is available from the collector.");
        value = row;
    }
    return rows;
}

QVariant PulseTelemetryController::remoteTelemetryValue(const char *name) const
{
    const QString key = QString::fromLatin1(name);
    const bool disconnected = !m_receivedAt.isValid() || m_connectionState.startsWith(QStringLiteral("Disconnected"));
    const bool stale = m_receivedAt.isValid() && m_receivedAt.elapsed() > std::max(5000, m_timer.interval() * 3);
    if (disconnected || stale) {
        if (key == QStringLiteral("verdictSeverity")) return static_cast<int>(VerdictSeverity::Unknown);
        if (key == QStringLiteral("verdictHeadline")) return stale ? QStringLiteral("Readings are stale") : QStringLiteral("Waiting for the collector");
        if (key == QStringLiteral("verdictCausalChain") || key == QStringLiteral("verdictRecommendation")) return QStringLiteral("No current diagnosis is available. Check the collector connection; retained history remains available.");
        if (key.endsWith(QStringLiteral("Available"))) return false;
    }
    if (!m_selectedGpuId.isEmpty() && key.startsWith(QStringLiteral("gpu"))) {
        const auto values = gpuValues(deviceWithId(m_remoteValues.value(QStringLiteral("gpuDevices")).toList(), m_selectedGpuId));
        if (values.contains(key)) return values.value(key);
    }
    if (!m_selectedStorageId.isEmpty() && (key == QStringLiteral("diskReadMibPerSec")
        || key == QStringLiteral("diskWriteMibPerSec") || key == QStringLiteral("diskThroughputAvailable")))
        return diskValues(deviceWithId(m_remoteValues.value(QStringLiteral("storageDevices")).toList(), m_selectedStorageId)).value(key);
    if (key == QStringLiteral("netRxMibPerSec") || key == QStringLiteral("netTxMibPerSec") || key == QStringLiteral("netThroughputAvailable"))
        return networkValues(m_remoteValues.value(QStringLiteral("networkInterfaces")).toList(), m_selectedNetworkInterfaces).value(key);
    if (key == QStringLiteral("metricMetadata")) {
        auto metadata = m_remoteValues.value(key).toMap();
        if (disconnected || stale) {
            for (auto it = metadata.begin(); it != metadata.end(); ++it) { auto metric = it.value().toMap(); metric[QStringLiteral("state")] = stale ? QStringLiteral("stale") : QStringLiteral("unavailable"); metric[QStringLiteral("detail")] = QStringLiteral("No current observation is available from the collector."); it.value() = metric; }
            return metadata;
        }
        if (!m_selectedGpuId.isEmpty()) {
            const auto gpu = deviceWithId(m_remoteValues.value(QStringLiteral("gpuDevices")).toList(), m_selectedGpuId);
            updateMetadata(metadata, QStringLiteral("gpuLoad"), gpu.value(QStringLiteral("busyAvailable")).toBool(), m_selectedGpuId);
            updateMetadata(metadata, QStringLiteral("gpuTemperature"), gpu.value(QStringLiteral("temperatureAvailable")).toBool(), m_selectedGpuId);
        }
        if (!m_selectedStorageId.isEmpty()) updateMetadata(metadata, QStringLiteral("diskThroughput"),
            remoteTelemetryValue("diskThroughputAvailable").toBool(), m_selectedStorageId);
        updateMetadata(metadata, QStringLiteral("netThroughput"), remoteTelemetryValue("netThroughputAvailable").toBool(),
            m_selectedNetworkInterfaces.isEmpty() ? QStringLiteral("physical interfaces") : m_selectedNetworkInterfaces.join(QStringLiteral(", ")));
        return metadata;
    }
    return m_remoteValues.value(key);
}

QVariantMap PulseTelemetryController::sampleForView(const FlightSample &sample, int index) const
{
    auto view = sampleToVariantMap(sample, index);
    auto quality = sample.metricQuality;
    auto metadata = quality.value(QStringLiteral("metricMetadata")).toMap();
    if (!m_selectedGpuId.isEmpty()) {
        const auto gpu = deviceWithId(sample.gpuDevices, m_selectedGpuId);
        const auto values = gpuValues(gpu);
        for (auto it = values.cbegin(); it != values.cend(); ++it) view[it.key()] = it.value();
        view[QStringLiteral("gpuBusy")] = values.value(QStringLiteral("gpuBusyPercent"));
        view[QStringLiteral("gpuTemp")] = values.value(QStringLiteral("gpuTempCelsius"));
        view[QStringLiteral("gpuFreqMhz")] = values.value(QStringLiteral("gpuCurrentFreqMhz"));
        view[QStringLiteral("gpuVramUsedMegabytes")] = values.value(QStringLiteral("gpuVramUsedMib"));
        view[QStringLiteral("gpuVramTotalMegabytes")] = values.value(QStringLiteral("gpuVramTotalMib"));
        for (const auto &key : {QStringLiteral("gpuBusyAvailable"), QStringLiteral("gpuTempAvailable"), QStringLiteral("gpuFrequencyAvailable"), QStringLiteral("gpuPowerAvailable"), QStringLiteral("gpuVramAvailable")})
            quality[key] = values.value(key, false);
        updateMetadata(metadata, QStringLiteral("gpuLoad"), gpu.value(QStringLiteral("busyAvailable")).toBool(), m_selectedGpuId);
        updateMetadata(metadata, QStringLiteral("gpuTemperature"), gpu.value(QStringLiteral("temperatureAvailable")).toBool(), m_selectedGpuId);
    }
    if (!m_selectedStorageId.isEmpty()) {
        const auto values = diskValues(deviceWithId(sample.storageDevices, m_selectedStorageId));
        for (auto it = values.cbegin(); it != values.cend(); ++it) view[it.key()] = it.value();
        quality[QStringLiteral("diskThroughputAvailable")] = values.value(QStringLiteral("diskThroughputAvailable"));
        updateMetadata(metadata, QStringLiteral("diskThroughput"), values.value(QStringLiteral("diskThroughputAvailable")).toBool(), m_selectedStorageId);
    }
    // Old recordings without device arrays retain their original physical aggregate.
    // Explicit virtual-interface selections cannot be reconstructed from that aggregate.
    if (!sample.networkDevices.isEmpty() || !m_selectedNetworkInterfaces.isEmpty()) {
        const auto values = networkValues(sample.networkDevices, m_selectedNetworkInterfaces);
        for (auto it = values.cbegin(); it != values.cend(); ++it) view[it.key()] = it.value();
        quality[QStringLiteral("netThroughputAvailable")] = values.value(QStringLiteral("netThroughputAvailable"));
        updateMetadata(metadata, QStringLiteral("netThroughput"), values.value(QStringLiteral("netThroughputAvailable")).toBool(),
            m_selectedNetworkInterfaces.isEmpty() ? QStringLiteral("physical interfaces") : m_selectedNetworkInterfaces.join(QStringLiteral(", ")));
    }
    quality[QStringLiteral("metricMetadata")] = metadata;
    view[QStringLiteral("metricMetadata")] = metadata;
    view[QStringLiteral("metricQuality")] = quality;
    for (auto it = quality.cbegin(); it != quality.cend(); ++it) view[it.key()] = it.value();
    return view;
}

// Rich recordings stay native. QML receives only endpoints, one inspected sample,
// and per-pixel extrema; process observations are never converted for plotting.
QVariantMap PulseTelemetryController::historySummary() const
{
    const int count = m_historySamplesVariant.size();
    if (!count) return {{QStringLiteral("count"), 0}};
    const auto first = m_historySamplesVariant.first().toMap();
    const auto last = m_historySamplesVariant.last().toMap();
    return {{QStringLiteral("count"), count},
        {QStringLiteral("captureId"), first.value(QStringLiteral("captureId"))},
        {QStringLiteral("firstId"), first.value(QStringLiteral("sampleId"))},
        {QStringLiteral("firstTime"), first.value(QStringLiteral("timestampMs"))},
        {QStringLiteral("lastTime"), last.value(QStringLiteral("timestampMs"))},
        {QStringLiteral("firstLabel"), first.value(QStringLiteral("timeString"))},
        {QStringLiteral("middleLabel"), m_historySamplesVariant.at(count / 2).toMap().value(QStringLiteral("timeString"))},
        {QStringLiteral("lastLabel"), last.value(QStringLiteral("timeString"))}};
}

QVariantMap PulseTelemetryController::historySampleAt(int index) const
{
    return index >= 0 && index < m_historySamplesVariant.size() ? m_historySamplesVariant.at(index).toMap() : QVariantMap{};
}

int PulseTelemetryController::historyIndexAtRatio(double ratio) const
{
    const int count = m_historySamplesVariant.size();
    if (!count) return -1;
    const auto timeAt = [this](int i) { return m_historySamplesVariant.at(i).toMap().value(QStringLiteral("timestampMs")).toLongLong(); };
    const qint64 target = timeAt(0) + qBound(0.0, ratio, 1.0) * (timeAt(count - 1) - timeAt(0));
    int low = 0, high = count - 1;
    while (low < high) {
        const int middle = low + (high - low) / 2;
        if (timeAt(middle) < target) low = middle + 1;
        else high = middle;
    }
    return low > 0 && target - timeAt(low - 1) <= timeAt(low) - target ? low - 1 : low;
}

QVariantList PulseTelemetryController::historyRecentSeries(const QString &valueKey, const QString &availableKey, int limit) const
{
    QVariantList result;
    const int count = m_historySamplesVariant.size();
    const int start = qMax(0, count - qBound(0, limit, 256));
    result.reserve(count - start);
    for (int i = start; i < count; ++i) {
        const auto sample = m_historySamplesVariant.at(i).toMap();
        result.append(QVariantMap{{QStringLiteral("value"), sample.value(valueKey).toDouble()},
            {QStringLiteral("available"), sample.value(availableKey).toBool()},
            {QStringLiteral("gapBefore"), sample.value(QStringLiteral("gapBefore")).toBool()}});
    }
    return result;
}

QString PulseTelemetryController::historyPlotJson(int pixelWidth) const
{
    return QString::fromUtf8(QJsonDocument::fromVariant(historyPlot(pixelWidth)).toJson(QJsonDocument::Compact));
}

QString PulseTelemetryController::historyRecentSeriesJson(const QString &valueKey, const QString &availableKey, int limit) const
{
    return QString::fromUtf8(QJsonDocument::fromVariant(historyRecentSeries(valueKey, availableKey, limit)).toJson(QJsonDocument::Compact));
}

QVariantMap PulseTelemetryController::historyPlot(int pixelWidth) const
{
    QVariantMap result;
    const int count = m_historySamplesVariant.size();
    if (!count) { m_historyPlotSamples.clear(); return result; }
    const int columns = qBound(1, pixelWidth, 4096);
    const qint64 firstTime = m_historySamplesVariant.first().toMap().value(QStringLiteral("timestampMs")).toLongLong();
    const qint64 span = qMax<qint64>(1, m_historySamplesVariant.last().toMap().value(QStringLiteral("timestampMs")).toLongLong() - firstTime);
    struct Bucket { int first = -1, last = -1, low = -1, high = -1; double minimum = 0, maximum = 0; };
    const QList<QPair<QString, QString>> channels = {
        {QStringLiteral("cpuTemp"), QStringLiteral("cpuTempAvailable")},
        {QStringLiteral("cpuLoad"), QStringLiteral("cpuLoadAvailable")},
        {QStringLiteral("responsivenessScore"), QStringLiteral("psiAvailable")},
        {QStringLiteral("packageWatts"), QStringLiteral("packagePowerAvailable")},
        {QStringLiteral("batteryWatts"), QStringLiteral("batteryDischargeAvailable")},
        {QStringLiteral("gpuBusy"), QStringLiteral("gpuBusyAvailable")},
        {QStringLiteral("diskTotalMibPerSec"), QStringLiteral("diskThroughputAvailable")}};
    const quint64 firstId = m_historySamplesVariant.first().toMap().value(QStringLiteral("sampleId")).toULongLong();
    const quint64 lastId = m_historySamplesVariant.last().toMap().value(QStringLiteral("sampleId")).toULongLong();
    while (!m_historyPlotSamples.isEmpty() && m_historyPlotSamples.first().sampleId < firstId)
        m_historyPlotSamples.removeFirst();
    int start = 0;
    if (!m_historyPlotSamples.isEmpty()) {
        const auto &cached = m_historyPlotSamples;
        if (cached.first().sampleId == firstId && cached.last().sampleId <= lastId)
            start = m_recorder.indexOfSample(cached.last().sampleId) + 1;
        if (start <= 0 || start != cached.size()) { m_historyPlotSamples.clear(); start = 0; }
    }
    m_historyPlotSamples.reserve(qMax(16, count * 2));
    for (int i = start; i < count; ++i) {
        const auto sample = m_historySamplesVariant.at(i).toMap();
        HistoryPlotSample point;
        point.sampleId = sample.value(QStringLiteral("sampleId")).toULongLong();
        point.timestamp = sample.value(QStringLiteral("timestampMs")).toLongLong();
        point.gap = sample.value(QStringLiteral("gapBefore")).toBool();
        point.throttled = sample.value(QStringLiteral("throttled")).toBool();
        point.severity = sample.value(QStringLiteral("severity")).toInt();
        for (int channel = 0; channel < channels.size(); ++channel) {
            const auto &keys = channels.at(channel);
            point.values[channel] = channel == 6
                ? sample.value(QStringLiteral("diskReadMibPerSec")).toDouble() + sample.value(QStringLiteral("diskWriteMibPerSec")).toDouble()
                : sample.value(keys.first).toDouble();
            point.available[channel] = sample.value(keys.second).toBool() && std::isfinite(point.values[channel]);
        }
        m_historyPlotSamples.append(point);
    }
    const auto &samples = m_historyPlotSamples;
    QList<int> sampleColumns(count);
    for (int i = 0; i < count; ++i)
        sampleColumns[i] = qBound(0, int((samples.at(i).timestamp - firstTime) * double(columns) / span), columns - 1);
    int channelIndex = 0;
    for (const auto &channel : channels) {
        QList<Bucket> buckets(columns);
        QList<int> segments(count);
        int segment = 0;
        for (int i = 0; i < count; ++i) {
            const auto &sample = samples.at(i);
            auto &bucket = buckets[sampleColumns.at(i)];
            if (sample.gap) ++segment;
            const double value = sample.values[channelIndex];
            if (!sample.available[channelIndex]) { ++segment; continue; }
            segments[i] = segment;
            if (bucket.first < 0) { bucket.first = bucket.low = bucket.high = i; bucket.minimum = bucket.maximum = value; }
            bucket.last = i;
            if (value < bucket.minimum) { bucket.minimum = value; bucket.low = i; }
            if (value > bucket.maximum) { bucket.maximum = value; bucket.high = i; }
        }
        QVariantList points;
        int previousSegment = -1;
        for (const auto &bucket : buckets) {
            if (bucket.first < 0) continue;
            QList<int> indices{bucket.first, bucket.low, bucket.high, bucket.last};
            std::sort(indices.begin(), indices.end());
            indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
            for (int i : indices) {
                // Continuity is determined before reduction, so missing readings
                // and subpixel gaps cannot be bridged by discarded observations.
                points.append(QVariantMap{{QStringLiteral("timestampMs"), samples.at(i).timestamp},
                    {QStringLiteral("value"), samples.at(i).values[channelIndex]},
                    {QStringLiteral("gapBefore"), segments.at(i) != previousSegment}});
                previousSegment = segments.at(i);
            }
        }
        result.insert(channel.first, points);
        ++channelIndex;
    }
    QVariantList markers;
    int previousColumn = -1;
    qint64 markerTime = 0;
    bool markerGap = false, markerThrottled = false;
    int markerSeverity = 0;
    const auto flushMarker = [&] {
        if (previousColumn < 0) return;
        markers.append(QVariantMap{{QStringLiteral("timestampMs"), markerTime},
            {QStringLiteral("gapBefore"), markerGap}, {QStringLiteral("throttled"), markerThrottled},
            {QStringLiteral("severity"), markerSeverity}});
    };
    for (int i = 0; i < count; ++i) {
        const auto &sample = samples.at(i);
        const int column = sampleColumns.at(i);
        if (column != previousColumn) {
            flushMarker();
            markerTime = sample.timestamp;
            markerGap = false; markerThrottled = false; markerSeverity = 0;
            previousColumn = column;
        }
        markerGap |= sample.gap;
        markerThrottled |= sample.throttled;
        markerSeverity = qMax(markerSeverity, sample.severity);
    }
    flushMarker();
    result.insert(QStringLiteral("markers"), markers);
    return result;
}

void PulseTelemetryController::setSelectedGpuId(const QString &id)
{
    if (m_selectedGpuId == id) return;
    m_selectedGpuId = id;
    rebuildHistoryView();
    Q_EMIT telemetryUpdated();
    Q_EMIT selectedHistoryChanged();
}
void PulseTelemetryController::setSelectedStorageId(const QString &id)
{
    if (m_selectedStorageId == id) return;
    m_selectedStorageId = id;
    rebuildHistoryView();
    Q_EMIT telemetryUpdated();
    Q_EMIT selectedHistoryChanged();
}
}
