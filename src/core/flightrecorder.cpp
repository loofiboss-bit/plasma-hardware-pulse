#include "flightrecorder.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace LoofiPulse
{

FlightRecorder::FlightRecorder(int capacity)
    : m_capacity(std::max(10, capacity))
{
    m_samples.reserve(m_capacity);
}

void FlightRecorder::appendSample(const FlightSample &sample)
{
    if (m_samples.size() >= m_capacity) {
        m_samples.removeFirst();
    }
    m_samples.append(sample);
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
        if (m_samples.at(i).cpuTempCelsius > maxTemp) {
            maxTemp = m_samples.at(i).cpuTempCelsius;
            bestIdx = i;
        }
    }
    return bestIdx;
}

int FlightRecorder::worstResponsivenessIndex() const
{
    int worstIdx = -1;
    double minScore = 101.0;
    for (int i = 0; i < m_samples.size(); ++i) {
        if (m_samples.at(i).responsivenessScore < minScore) {
            minScore = m_samples.at(i).responsivenessScore;
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

    int throttleSamples = 0;
    for (const FlightSample &s : m_samples) {
        if (s.throttled) {
            ++throttleSamples;
        }
    }

    QString md;
    md += QStringLiteral("# Loofi Pulse — Causal Flight Recorder Report\n\n");
    md += QStringLiteral("- **Recorded Samples**: %1 / %2s\n").arg(m_samples.size()).arg(m_capacity);
    md += QStringLiteral("- **Throttle Intervals Detected**: %1\n").arg(throttleSamples);
    md += QStringLiteral("- **Latest Verdict**: %1\n").arg(latest.verdictHeadline);
    md += QStringLiteral("- **Latest CPU / Power**: %1°C, %2% load @ %3 MHz (~%4 W)\n\n")
              .arg(latest.cpuTempCelsius, 0, 'f', 1)
              .arg(latest.cpuLoadPercent, 0, 'f', 1)
              .arg(latest.cpuFreqMhz)
              .arg(latest.systemWatts, 0, 'f', 1);

    if (peakIdx >= 0) {
        const FlightSample &p = m_samples.at(peakIdx);
        const QString tStr = QDateTime::fromMSecsSinceEpoch(p.timestampMs).toString(QStringLiteral("HH:mm:ss"));
        md += QStringLiteral("## Peak Thermal Spike (%1)\n").arg(tStr);
        md += QStringLiteral("- **CPU Package Temp**: %1°C (%2% load @ %3 MHz)\n")
                  .arg(p.cpuTempCelsius, 0, 'f', 1)
                  .arg(p.cpuLoadPercent, 0, 'f', 1)
                  .arg(p.cpuFreqMhz);
        md += QStringLiteral("- **Driving Process**: %1 (PID %2, %3% CPU, %4 MiB RSS)\n\n")
                  .arg(p.culpritName.isEmpty() ? QStringLiteral("idle") : p.culpritName)
                  .arg(p.culpritPid)
                  .arg(p.culpritCpuPercent, 0, 'f', 1)
                  .arg(p.culpritRssMb, 0, 'f', 0);
    }

    if (worstIdx >= 0) {
        const FlightSample &w = m_samples.at(worstIdx);
        const QString tStr = QDateTime::fromMSecsSinceEpoch(w.timestampMs).toString(QStringLiteral("HH:mm:ss"));
        md += QStringLiteral("## Worst PSI Stall / Responsiveness Drop (%1)\n").arg(tStr);
        md += QStringLiteral("- **PSI Responsiveness Score**: %1% (CPU some=%2%, Mem some=%3%, IO full=%4%)\n")
                  .arg(w.responsivenessScore, 0, 'f', 1)
                  .arg(w.psiCpuSome10, 0, 'f', 2)
                  .arg(w.psiMemSome10, 0, 'f', 2)
                  .arg(w.psiIoFull10, 0, 'f', 2);
        md += QStringLiteral("- **Driving Process**: %1 (PID %2)\n")
                  .arg(w.culpritName.isEmpty() ? QStringLiteral("idle") : w.culpritName)
                  .arg(w.culpritPid);
    }

    return md;
}

QString FlightRecorder::exportSamplesJson() const
{
    QJsonArray arr;
    for (int i = 0; i < m_samples.size(); ++i) {
        const FlightSample &s = m_samples.at(i);
        QJsonObject obj;
        obj.insert(QStringLiteral("index"), i);
        obj.insert(QStringLiteral("timestampMs"), s.timestampMs);
        obj.insert(QStringLiteral("cpuTempCelsius"), s.cpuTempCelsius);
        obj.insert(QStringLiteral("cpuLoadPercent"), s.cpuLoadPercent);
        obj.insert(QStringLiteral("cpuFreqMhz"), s.cpuFreqMhz);
        obj.insert(QStringLiteral("gpuBusyPercent"), s.gpuBusyPercent);
        obj.insert(QStringLiteral("responsivenessScore"), s.responsivenessScore);
        obj.insert(QStringLiteral("systemWatts"), s.systemWatts);
        obj.insert(QStringLiteral("throttled"), s.throttled);
        obj.insert(QStringLiteral("severity"), static_cast<int>(s.severity));
        obj.insert(QStringLiteral("verdictHeadline"), s.verdictHeadline);
        obj.insert(QStringLiteral("culpritName"), s.culpritName);
        obj.insert(QStringLiteral("culpritPid"), s.culpritPid);
        obj.insert(QStringLiteral("culpritCpuPercent"), s.culpritCpuPercent);
        obj.insert(QStringLiteral("culpritRssMb"), s.culpritRssMb);
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

} // namespace LoofiPulse
