#include "pulsetelemetrycontroller.h"
#include "kernellogreader.h"
#include "sessionstore.h"
#include <QDateTime>
#include <QPair>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QSet>
#include <QRunnable>
#include <QSaveFile>
#include <QStandardPaths>
namespace LoofiPulse {

static qint64 wallTimeForSample(const FlightRecorder &recorder, quint64 sampleId)
{
    const int index = recorder.indexOfSample(sampleId);
    return index >= 0 ? recorder.sampleAt(index).timestampMs : 0;
}

static QPair<qint64, qint64> reportWallTimeRange(const FlightRecorder &recorder, const QVariantMap &selection)
{
    const QString incidentId = selection.value(QStringLiteral("incidentId")).toString();
    if (!incidentId.isEmpty()) {
        const auto range = recorder.incidentWallTimeRange(incidentId);
        if (range.first > 0) return range;
    }
    const QString markerId = selection.value(QStringLiteral("markerId")).toString();
    if (!markerId.isEmpty()) {
        const auto range = recorder.incidentWallTimeRange(markerId);
        if (range.first > 0) return range;
    }
    const quint64 startId = selection.value(QStringLiteral("selectionStartSampleId")).toULongLong();
    const quint64 endId = selection.value(QStringLiteral("selectionEndSampleId")).toULongLong();
    if (startId && endId) {
        const qint64 start = wallTimeForSample(recorder, startId);
        const qint64 end = wallTimeForSample(recorder, endId);
        if (start > 0 && end >= start) return {start, end};
    }
    if (recorder.count() > 0)
        return {recorder.sampleAt(0).timestampMs, recorder.sampleAt(recorder.count() - 1).timestampMs};
    return {};
}

static QVariantList correlatedKernelLogs(const FlightRecorder &recorder, const QVariantMap &selection)
{
    const auto range = reportWallTimeRange(recorder, selection);
    if (range.first <= 0 || range.second < range.first) return {};
    QVariantList events = KernelLogReader::fetchIncidentLogsVariant(range.first, range.second, 15000);
    const QString selectedIncident = selection.value(QStringLiteral("incidentId")).toString();
    const QString selectedMarker = selection.value(QStringLiteral("markerId")).toString();
    const quint64 startId = selection.value(QStringLiteral("selectionStartSampleId")).toULongLong();
    const quint64 endId = selection.value(QStringLiteral("selectionEndSampleId")).toULongLong();
    for (auto &value : events) {
        QVariantMap event = value.toMap();
        const qint64 eventMs = event.value(QStringLiteral("epochMs")).toLongLong();
        QStringList incidentIds;
        if (!selectedIncident.isEmpty()) {
            incidentIds.append(selectedIncident);
        } else if (!selectedMarker.isEmpty()) {
            incidentIds.append(selectedMarker);
        } else if (startId && endId) {
            event.insert(QStringLiteral("selectionStartSampleId"), QString::number(startId));
            event.insert(QStringLiteral("selectionEndSampleId"), QString::number(endId));
        } else {
            for (const auto &episodeValue : recorder.incidentEpisodes()) {
                const QString id = episodeValue.toMap().value(QStringLiteral("incidentId")).toString();
                const auto incidentRange = recorder.incidentWallTimeRange(id);
                if (!id.isEmpty() && eventMs > 0 && incidentRange.first > 0
                    && eventMs >= incidentRange.first - 15000 && eventMs <= incidentRange.second + 15000)
                    incidentIds.append(id);
            }
            for (const auto &markerValue : recorder.markers()) {
                const QVariantMap marker = markerValue.toMap();
                const QString id = marker.value(QStringLiteral("markerId")).toString();
                const qint64 markerMs = marker.value(QStringLiteral("timestampMs")).toLongLong();
                if (!id.isEmpty() && markerMs > 0 && eventMs >= markerMs - 15000 && eventMs <= markerMs + 15000)
                    incidentIds.append(id);
            }
        }
        incidentIds.removeDuplicates();
        event.insert(QStringLiteral("incidentIds"), incidentIds);
        if (incidentIds.size() == 1)
            event.insert(QStringLiteral("incidentId"), incidentIds.first());
        value = event;
    }
    return events;
}

static QString markdownInline(QString value)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('|'), QStringLiteral("\\|"));
    value.replace(QLatin1Char('\r'), QLatin1Char(' '));
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    value.replace(QLatin1Char('`'), QLatin1Char('\''));
    return value;
}

static QString buildHtmlReport(const FlightRecorder &recorder, const QString &markdownText, const QVariantList &kernelLogs, const QVariantMap &comparison)
{
    QString html;
    html.reserve(16384);
    html += QStringLiteral("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
                           "<meta charset=\"utf-8\">\n"
                           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
                           "<title>Loofi Pulse Incident Report</title>\n"
                           "<style>\n"
                           ":root { --bg: #1a1d24; --card: #222631; --card-nested: #161820; --text: #f0f2f5; "
                           "--text-dim: #9aa0a6; --border: rgba(255,255,255,0.08); --accent: #00b4d8; "
                           "--positive: #27ae60; --warning: #f67400; --negative: #da4453; }\n"
                           "body { background: var(--bg); color: var(--text); font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Noto Sans', Ubuntu, sans-serif; margin: 0; padding: 24px; line-height: 1.5; }\n"
                           ".container { max-width: 1040px; margin: 0 auto; }\n"
                           ".header { display: flex; align-items: center; justify-content: space-between; border-bottom: 1px solid var(--border); padding-bottom: 16px; margin-bottom: 24px; }\n"
                           ".card { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 16px rgba(0,0,0,0.25); }\n"
                           ".badge { display: inline-block; padding: 3px 10px; border-radius: 999px; font-size: 12px; font-weight: 600; }\n"
                           ".badge-nominal { background: rgba(39,174,96,0.18); color: var(--positive); border: 1px solid var(--positive); }\n"
                           "table { width: 100%; border-collapse: collapse; margin-top: 12px; font-size: 14px; }\n"
                           "th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--border); }\n"
                           "th { color: var(--text-dim); font-size: 12px; text-transform: uppercase; letter-spacing: 0.5px; }\n"
                           ".chart-box { background: var(--card-nested); border-radius: 8px; padding: 16px; margin-top: 16px; overflow-x: auto; }\n"
                           "svg text { font-family: sans-serif; font-size: 10px; fill: var(--text-dim); }\n"
                           "code, pre { font-family: 'JetBrains Mono', 'Fira Code', monospace; background: var(--card-nested); padding: 2px 6px; border-radius: 4px; font-size: 13px; }\n"
                           "pre { padding: 14px; overflow-x: auto; white-space: pre-wrap; }\n"
                           "</style>\n</head>\n<body>\n<div class=\"container\">\n");

    html += QStringLiteral("<div class=\"header\"><div><h1>Loofi Pulse Incident Report</h1>"
                           "<div style=\"color:var(--text-dim);font-size:14px;\">Recording ID: %1 · Generated %2</div></div>"
                           "<div class=\"badge badge-nominal\">Loofi Pulse v%3</div></div>\n")
                .arg(recorder.recordingId().toHtmlEscaped(), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                     QStringLiteral(LOOFI_PULSE_VERSION));

    const auto &samples = recorder.samples();
    if (samples.size() >= 2) {
        const int w = 960, h = 140, pad = 36;
        const int plotW = w - pad * 2, plotH = h - pad * 2;
        const int n = samples.size();
        const auto buildPath = [&](const QString &availabilityKey, auto value, double maximum) {
            QString path;
            bool startSegment = true;
            for (int i = 0; i < n; ++i) {
                const auto &sample = samples.at(i);
                if (!sample.metricQuality.value(availabilityKey).toBool()) {
                    startSegment = true;
                    continue;
                }
                const double x = pad + (static_cast<double>(i) / (n - 1)) * plotW;
                const double y = (pad + plotH) - std::clamp(value(sample) / maximum, 0.0, 1.0) * plotH;
                path += QStringLiteral("%1%2,%3 ").arg(startSegment ? QStringLiteral("M") : QStringLiteral("L"))
                    .arg(x, 0, 'f', 1).arg(y, 0, 'f', 1);
                startSegment = false;
            }
            return path.trimmed();
        };
        const QString cpuPath = buildPath(QStringLiteral("cpuLoadAvailable"), [](const FlightSample &s) { return s.cpuLoadPercent; }, 100.0);
        const QString tempPath = buildPath(QStringLiteral("cpuTempAvailable"), [](const FlightSample &s) { return s.cpuTempCelsius; }, 105.0);
        const QString respPath = buildPath(QStringLiteral("psiAvailable"), [](const FlightSample &s) { return s.responsivenessScore; }, 100.0);

        html += QStringLiteral("<div class=\"card\"><h2>Telemetry Timeline (%1 samples)</h2>"
                               "<div class=\"chart-box\"><svg viewBox=\"0 0 %1 %2\" width=\"100%\" height=\"%2\">"
                               "<line x1=\"%3\" y1=\"%4\" x2=\"%5\" y2=\"%4\" stroke=\"var(--border)\" stroke-width=\"1\"/>"
                               "<line x1=\"%3\" y1=\"%6\" x2=\"%5\" y2=\"%6\" stroke=\"var(--border)\" stroke-width=\"1\"/>"
                               "<text x=\"4\" y=\"%4\">100%</text><text x=\"4\" y=\"%6\">0%</text>"
                               "<path fill=\"none\" stroke=\"var(--accent)\" stroke-width=\"2\" d=\"%7\"/>"
                               "<path fill=\"none\" stroke=\"var(--warning)\" stroke-width=\"2\" d=\"%8\"/>"
                               "<path fill=\"none\" stroke=\"var(--positive)\" stroke-width=\"2\" d=\"%9\"/>"
                               "</svg></div>"
                               "<div style=\"display:flex;gap:18px;margin-top:12px;font-size:13px;\">"
                               "<div><span style=\"color:var(--accent);font-weight:bold;\">―</span> CPU Load (%)</div>"
                               "<div><span style=\"color:var(--warning);font-weight:bold;\">―</span> CPU Temp (°C)</div>"
                               "<div><span style=\"color:var(--positive);font-weight:bold;\">―</span> Responsiveness (Score/100)</div>"
                               "<div style=\"color:var(--text-dim);\">Unavailable samples are omitted.</div>"
                               "</div></div>\n")
                    .arg(w).arg(h).arg(pad).arg(pad).arg(w - pad).arg(pad + plotH)
                    .arg(cpuPath, tempPath, respPath);
    }

    if (!comparison.isEmpty()) {
        html += QStringLiteral("<div class=\"card\"><h2>Incident Comparison Analysis</h2><table>"
                               "<thead><tr><th>Period</th><th>Samples</th><th>CPU Avg</th><th>Responsiveness</th><th>Coverage</th><th>Completeness</th></tr></thead><tbody>");
        for (const auto &period : {QStringLiteral("before"), QStringLiteral("during"), QStringLiteral("after")}) {
            const auto p = comparison.value(period).toMap();
            if (!p.isEmpty()) {
                html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td><td>%3</td><td>%4</td><td>%5 ms</td><td>%6</td></tr>")
                    .arg(period)
                    .arg(p.value(QStringLiteral("sampleCount")).toInt())
                    .arg(p.value(QStringLiteral("cpuLoadAvailable")).toBool()
                             ? QStringLiteral("%1%").arg(p.value(QStringLiteral("cpuLoadPercent")).toDouble(), 0, 'f', 1)
                             : QStringLiteral("Unavailable"))
                    .arg(p.value(QStringLiteral("responsivenessAvailable")).toBool()
                             ? QStringLiteral("%1/100").arg(p.value(QStringLiteral("responsivenessScore")).toDouble(), 0, 'f', 1)
                             : QStringLiteral("Unavailable"))
                    .arg(p.value(QStringLiteral("coverageMs")).toLongLong())
                    .arg(p.value(QStringLiteral("complete")).toBool() ? QStringLiteral("Complete") : QStringLiteral("Partial"));
            }
        }
        html += QStringLiteral("</tbody></table></div>\n");
    }

    if (!kernelLogs.isEmpty()) {
        html += QStringLiteral("<div class=\"card\"><h2>Correlated Kernel Events (±15s context, privacy-redacted)</h2><table>"
                               "<thead><tr><th>Time</th><th>Incident</th><th>Subsystem</th><th>Priority</th><th>Message</th></tr></thead><tbody>");
        for (const auto &entryVal : kernelLogs) {
            const auto entry = entryVal.toMap();
            const qint64 timestamp = entry.value(QStringLiteral("timestampMs"), entry.value(QStringLiteral("epochMs"))).toLongLong();
            const QString timeStr = timestamp > 0
                ? QDateTime::fromMSecsSinceEpoch(timestamp).toLocalTime().toString(QStringLiteral("HH:mm:ss"))
                : entry.value(QStringLiteral("timestamp")).toString();
            QStringList ids = entry.value(QStringLiteral("incidentIds")).toStringList();
            const QString directId = entry.value(QStringLiteral("incidentId")).toString();
            if (!directId.isEmpty() && !ids.contains(directId)) ids.append(directId);
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td><code>%3</code></td><td>%4</td><td>%5</td></tr>")
                .arg(timeStr.toHtmlEscaped())
                .arg(ids.isEmpty() ? QStringLiteral("—") : ids.join(QStringLiteral(", ")).toHtmlEscaped())
                .arg(entry.value(QStringLiteral("subsystem")).toString().toUpper().toHtmlEscaped())
                .arg(entry.value(QStringLiteral("severity")).toString().toHtmlEscaped())
                .arg(entry.value(QStringLiteral("message")).toString().toHtmlEscaped());
        }
        html += QStringLiteral("</tbody></table></div>\n");
    }

    html += QStringLiteral("<div class=\"card\"><h2>Markdown Summary Export</h2><pre>") + markdownText.toHtmlEscaped() + QStringLiteral("</pre></div>\n");
    html += QStringLiteral("</div></body></html>\n");
    return html;
}

void PulseTelemetryController::exportRecorderAsync(const FlightRecorder &recorder, const QVariantMap &selection)
{
    if (m_actionPending) return;
    QVariantMap safeSelection;
    for (const auto &key : {QStringLiteral("selectionStartSampleId"), QStringLiteral("selectionEndSampleId")})
        if (selection.contains(key)) safeSelection[key] = QString::number(selection.value(key).toULongLong());
    for (const auto &key : {QStringLiteral("incidentId"), QStringLiteral("markerId")})
        if (selection.contains(key) && selection.value(key).toString().size() <= 200)
            safeSelection[key] = selection.value(key).toString();
    const auto comparison = selection.value(QStringLiteral("comparison")).toMap();
    const QSet<QString> comparisonFields{QStringLiteral("sampleCount"), QStringLiteral("available"), QStringLiteral("coverageMs"),
        QStringLiteral("requestedMs"), QStringLiteral("gapCount"), QStringLiteral("complete"), QStringLiteral("cpuLoadAvailable"),
        QStringLiteral("cpuLoadPercent"), QStringLiteral("responsivenessAvailable"), QStringLiteral("responsivenessScore")};
    QVariantMap safeComparison;
    for (const auto &period : {QStringLiteral("before"), QStringLiteral("during"), QStringLiteral("after")}) {
        const auto values = comparison.value(period).toMap(); QVariantMap safe;
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            if (comparisonFields.contains(it.key()) && (it.value().metaType().id() == QMetaType::Bool || it.value().metaType().id() == QMetaType::Double
                || it.value().metaType().id() == QMetaType::LongLong || it.value().metaType().id() == QMetaType::Int))
                safe[it.key()] = it.value();
        safeComparison[period] = safe;
    }
    if (!safeComparison.isEmpty()) safeSelection[QStringLiteral("comparison")] = safeComparison;
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const bool include = m_exportIncludeProcessDetails;
    const auto filters = QVariantMap{{QStringLiteral("gpuId"), m_selectedGpuId}, {QStringLiteral("storageId"), m_selectedStorageId},
        {QStringLiteral("networkInterfaces"), m_selectedNetworkInterfaces}};
    QPointer<PulseTelemetryController> guard(this);
    m_actionPending = true; Q_EMIT telemetryUpdated();
    m_filePool.start(QRunnable::create([guard, recorder, selection = safeSelection, comparison = safeComparison, filters, base, include] {
        const QString directory = QDir(base).filePath(QStringLiteral("reports"));
        QString status, saved;
        bool succeeded = false;
        if (base.isEmpty() || (!QDir().mkpath(directory) && !QDir(directory).exists())) status = QStringLiteral("The reports folder could not be created.");
        else {
            const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
            const QString htmlPath = QDir(directory).filePath(QStringLiteral("loofi-pulse-report-%1.html").arg(stamp));
            const QString markdownPath = QDir(directory).filePath(QStringLiteral("loofi-pulse-report-%1.md").arg(stamp));
            const QString jsonPath = QDir(directory).filePath(QStringLiteral("loofi-pulse-samples-%1.json").arg(stamp));
            QVariantList kernelLogs = correlatedKernelLogs(recorder, selection);
            kernelLogs = SessionStore::redactReportText(kernelLogs, recorder.samples(), include).toList();
            auto root = QJsonDocument::fromJson(recorder.exportSamplesJson(include).toUtf8()).array();
            for (int i = 0; i < root.size(); ++i) {
                auto sample = root.at(i).toObject();
                sample.insert(QStringLiteral("selection"), QJsonObject::fromVariantMap(selection));
                sample.insert(QStringLiteral("viewSelection"), QJsonObject::fromVariantMap(filters));
                QJsonArray networkDevices;
                for (const QJsonValue &deviceValue : sample.value(QStringLiteral("networkDevices")).toArray()) {
                    QJsonObject device = deviceValue.toObject();
                    for (const QString &key : {QStringLiteral("wirelessSsid"), QStringLiteral("ssid"), QStringLiteral("bssid"),
                                               QStringLiteral("macAddress"), QStringLiteral("ipAddress"), QStringLiteral("ipv4"), QStringLiteral("ipv6")})
                        device.remove(key);
                    networkDevices.append(device);
                }
                if (!networkDevices.isEmpty()) sample.insert(QStringLiteral("networkDevices"), networkDevices);
                if (i == 0) {
                    sample.insert(QStringLiteral("correlatedKernelEvents"), QJsonArray::fromVariantList(kernelLogs));
                    QJsonObject context;
                    context.insert(QStringLiteral("recordingId"), recorder.recordingId());
                    context.insert(QStringLiteral("engineVersion"), QStringLiteral(LOOFI_PULSE_VERSION));
                    context.insert(QStringLiteral("selectionStartSampleId"), selection.value(QStringLiteral("selectionStartSampleId")).toString());
                    context.insert(QStringLiteral("selectionEndSampleId"), selection.value(QStringLiteral("selectionEndSampleId")).toString());
                    context.insert(QStringLiteral("kernelEventsAvailable"), !kernelLogs.isEmpty());
                    const QString incidentId = selection.value(QStringLiteral("incidentId")).toString();
                    if (!incidentId.isEmpty()) context.insert(QStringLiteral("incidentId"), incidentId);
                    sample.insert(QStringLiteral("reportContext"), context);
                }
                root[i] = sample;
            }
            root = QJsonArray::fromVariantList(SessionStore::redactReportText(root.toVariantList(), recorder.samples(), include).toList());
            QString markdown = recorder.exportSummaryMarkdown(include);
            if (!selection.isEmpty()) markdown += QStringLiteral("\n## Selected investigation\n\nOriginal selection sample IDs: %1–%2. The report includes available before/after context; missing context is not inferred.\n")
                .arg(selection.value(QStringLiteral("selectionStartSampleId")).toString(), selection.value(QStringLiteral("selectionEndSampleId")).toString());

            const auto comp = selection.value(QStringLiteral("comparison")).toMap();
            if (!comp.isEmpty()) {
                markdown += QStringLiteral("\n## Incident Comparison Analysis\n\n");
                for (const auto &period : {QStringLiteral("before"), QStringLiteral("during"), QStringLiteral("after")}) {
                    const auto p = comp.value(period).toMap();
                    if (!p.isEmpty()) {
                        markdown += QStringLiteral("- **%1**: %2 samples; CPU avg: %3; Responsiveness: %4; Coverage: %5 ms; %6\n")
                            .arg(period)
                            .arg(p.value(QStringLiteral("sampleCount")).toInt())
                            .arg(p.value(QStringLiteral("cpuLoadAvailable")).toBool()
                                     ? QStringLiteral("%1%").arg(p.value(QStringLiteral("cpuLoadPercent")).toDouble(), 0, 'f', 1)
                                     : QStringLiteral("Unavailable"))
                            .arg(p.value(QStringLiteral("responsivenessAvailable")).toBool()
                                     ? QStringLiteral("%1/100").arg(p.value(QStringLiteral("responsivenessScore")).toDouble(), 0, 'f', 1)
                                     : QStringLiteral("Unavailable"))
                            .arg(p.value(QStringLiteral("coverageMs")).toLongLong())
                            .arg(p.value(QStringLiteral("complete")).toBool() ? QStringLiteral("Complete coverage") : QStringLiteral("Partial coverage"));
                    }
                }
            }

            markdown = SessionStore::redactReportText(markdown, recorder.samples(), include).toString();

            markdown += QStringLiteral("\n## Correlated Kernel Events (±15s context, privacy-redacted)\n\n");
            if (kernelLogs.isEmpty()) {
                markdown += QStringLiteral("No matching hardware-related kernel messages were returned. Journal access may be unavailable.\n");
            } else {
                for (const auto &entryVal : kernelLogs) {
                    const auto entry = entryVal.toMap();
                    const qint64 timestamp = entry.value(QStringLiteral("timestampMs"), entry.value(QStringLiteral("epochMs"))).toLongLong();
                    const QString timeStr = timestamp > 0
                        ? QDateTime::fromMSecsSinceEpoch(timestamp).toLocalTime().toString(QStringLiteral("HH:mm:ss"))
                        : entry.value(QStringLiteral("timestamp")).toString();
                    QStringList ids = entry.value(QStringLiteral("incidentIds")).toStringList();
                    const QString directId = entry.value(QStringLiteral("incidentId")).toString();
                    if (!directId.isEmpty() && !ids.contains(directId)) ids.append(directId);
                    markdown += QStringLiteral("- [%1] [%2] %3 (priority: %4; incident IDs: %5)\n")
                        .arg(markdownInline(timeStr), markdownInline(entry.value(QStringLiteral("subsystem")).toString().toUpper()),
                             markdownInline(entry.value(QStringLiteral("message")).toString()),
                             markdownInline(entry.value(QStringLiteral("severity")).toString()),
                             markdownInline(ids.isEmpty() ? QStringLiteral("None") : ids.join(QStringLiteral(", "))));
                }
            }

            const QString html = buildHtmlReport(recorder, markdown, kernelLogs, comparison);

            const auto write = [](const QString &path, const QByteArray &bytes, QString &error) {
                QSaveFile file(path);
                if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { error = file.errorString(); return false; }
                return true;
            };
            QString markdownError, jsonError, htmlError;
            const bool md = write(markdownPath, markdown.toUtf8(), markdownError);
            const bool js = write(jsonPath, QJsonDocument(root).toJson(QJsonDocument::Indented), jsonError);
            const bool ht = write(htmlPath, html.toUtf8(), htmlError);
            succeeded = md && js && ht;
            saved = ht ? htmlPath : (md ? markdownPath : (js ? jsonPath : QString()));
            if (succeeded) status = QStringLiteral("Saved HTML, Markdown, and JSON reports.");
            else if (md || ht) status = QStringLiteral("Report saved at %1; partial error: %2").arg(saved, jsonError.isEmpty() ? htmlError : jsonError);
            else status = QStringLiteral("Report export failed. Markdown: %1. JSON: %2. HTML: %3").arg(markdownError, jsonError, htmlError);
        }
        if (!guard) return;
        QMetaObject::invokeMethod(guard, [guard, succeeded, status, saved, directory] {
            if (!guard) return;
            guard->m_actionPending = false;
            if (!saved.isEmpty()) guard->m_reportsDirectory = directory;
            guard->setActionResult(succeeded, status, saved);
            Q_EMIT guard->telemetryUpdated();
        }, Qt::QueuedConnection);
    }));
}
}
