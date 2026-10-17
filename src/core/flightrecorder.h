#pragma once

#include "telemetrytypes.h"

#include <QList>

namespace LoofiPulse
{

class FlightRecorder
{
public:
    explicit FlightRecorder(int capacity = 1200);

    quint64 appendSample(FlightSample sample);
    void clear();
    void setRetentionMs(qint64 retentionMs);

    int count() const;
    int capacity() const;
    int indexOfSample(quint64 sampleId) const;
    const QList<FlightSample> &samples() const;
    FlightSample sampleAt(int index) const;
    int peakTemperatureIndex() const;
    int worstResponsivenessIndex() const;

    QString exportSummaryMarkdown() const;
    QString exportSamplesJson() const;

private:
    int m_capacity = 1200;
    qint64 m_retentionMs = 5 * 60 * 1000;
    quint64 m_nextSampleId = 1;
    QList<FlightSample> m_samples;
};

} // namespace LoofiPulse
