#pragma once

#include "sessionstore.h"

#include <QList>

namespace LoofiPulse
{

class FlightRecorder
{
public:
    explicit FlightRecorder(int capacity = 7201);

    quint64 appendSample(FlightSample sample);
    void clear();
    // Restore an already validated frozen session without recomputing its diagnoses or identities.
    void restoreSnapshot(const SessionSnapshot &snapshot);
    void setRetentionMs(qint64 retentionMs);
    qint64 retentionMs() const;

    int count() const;
    int capacity() const;
    void setCapacity(int capacity);
    int indexOfSample(quint64 sampleId) const;
    const QList<FlightSample> &samples() const;
    FlightSample sampleAt(int index) const;
    int peakTemperatureIndex() const;
    int worstResponsivenessIndex() const;

    QVariantList incidentEpisodes() const;
    QString markProblemNow(const QString &label = QString(), bool isAutomated = false);
    QString markAutomatedIncident(const QString &ruleId, const QString &headline, qint64 timestampMs);
    SessionSnapshot freezeRange(quint64 startSampleId, quint64 endSampleId) const;
    SessionSnapshot freezeIncident(const QString &incidentId) const;
    QVariantMap compareRange(quint64 startSampleId, quint64 endSampleId) const;
    QVariantMap compareIncidents(const QString &incidentIdA, const QString &incidentIdB) const;
    QVariantList markers() const;
    QString recordingId() const;
    QString exportSummaryMarkdown(bool includeProcessDetails = false) const;
    QString exportSamplesJson(bool includeProcessDetails = false) const;
    static QVariantMap sampleMetricMetadata(const FlightSample &s);

private:
    void updateIncidents(const FlightSample &sample);
    void trimRecords();
    QString m_recordingId;
    QString m_engineVersion = QStringLiteral(LOOFI_PULSE_VERSION);
    QVariantList m_incidents;
    QVariantList m_markers;
    QVariantMap m_activeIncident;
    QMap<QString, qint64> m_lastAutoBookmarkMs;
    int m_peakRank = -1;
    int m_capacity = 7201;
    qint64 m_retentionMs = 5 * 60 * 1000;
    quint64 m_nextSampleId = 1;
    QList<FlightSample> m_samples;
};

} // namespace LoofiPulse
