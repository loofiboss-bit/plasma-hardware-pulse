#pragma once

#include "telemetrytypes.h"

#include <QList>

namespace LoofiPulse
{

class FlightRecorder
{
public:
    explicit FlightRecorder(int capacity = 3600);

    quint64 appendSample(FlightSample sample);
    void clear();
    void setRetentionMs(qint64 retentionMs);
    qint64 retentionMs() const;

    int count() const;
    int capacity() const;
    int indexOfSample(quint64 sampleId) const;
    const QList<FlightSample> &samples() const;
    FlightSample sampleAt(int index) const;
    int peakTemperatureIndex() const;
    int worstResponsivenessIndex() const;

    QString exportSummaryMarkdown() const;
    QString exportSamplesJson() const;
    static QVariantMap sampleMetricMetadata(const FlightSample &s);

private:
    int m_capacity = 3600;
    qint64 m_retentionMs = 5 * 60 * 1000;
    quint64 m_nextSampleId = 1;
    QList<FlightSample> m_samples;
};

} // namespace LoofiPulse
