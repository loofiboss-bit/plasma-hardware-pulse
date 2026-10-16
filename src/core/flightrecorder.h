#pragma once

#include "telemetrytypes.h"

#include <QList>

namespace LoofiPulse
{

class FlightRecorder
{
public:
    explicit FlightRecorder(int capacity = 300);

    void appendSample(const FlightSample &sample);
    void clear();

    int count() const;
    int capacity() const;
    const QList<FlightSample> &samples() const;
    FlightSample sampleAt(int index) const;
    int peakTemperatureIndex() const;
    int worstResponsivenessIndex() const;

private:
    int m_capacity = 300;
    QList<FlightSample> m_samples;
};

} // namespace LoofiPulse
