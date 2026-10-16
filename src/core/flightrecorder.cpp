#include "flightrecorder.h"

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

} // namespace LoofiPulse
