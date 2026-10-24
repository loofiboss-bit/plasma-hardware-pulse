#include "pulsetelemetrycontroller.h"
#include <QDateTime>
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
                           "<div class=\"badge badge-nominal\">Loofi Pulse v13</div></div>\n")
                .arg(recorder.recordingId(), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

    const auto &samples = recorder.samples();
    if (samples.size() >= 2) {
        const int w = 960, h = 140, pad = 36;
        const int plotW = w - pad * 2, plotH = h - pad * 2;
        QString cpuPoints, tempPoints, respPoints;
        const int n = samples.size();
        for (int i = 0; i < n; ++i) {
            const auto &s = samples.at(i);
            const double x = pad + (static_cast<double>(i) / (n - 1)) * plotW;
            const double cpuY = (pad + plotH) - std::clamp(s.cpuLoadPercent / 100.0, 0.0, 1.0) * plotH;
            const double tempY = (pad + plotH) - std::clamp(s.cpuTempCelsius / 105.0, 0.0, 1.0) * plotH;
            const double respY = (pad + plotH) - std::clamp(s.responsivenessScore / 100.0, 0.0, 1.0) * plotH;
            cpuPoints += QStringLiteral("%1,%2 ").arg(x, 0, 'f', 1).arg(cpuY, 0, 'f', 1);
            tempPoints += QStringLiteral("%1,%2 ").arg(x, 0, 'f', 1).arg(tempY, 0, 'f', 1);
            respPoints += QStringLiteral("%1,%2 ").arg(x, 0, 'f', 1).arg(respY, 0, 'f', 1);
        }

        html += QStringLiteral("<div class=\"card\"><h2>Telemetry Timeline (%1 samples)</h2>"
                               "<div class=\"chart-box\"><svg viewBox=\"0 0 %1 %2\" width=\"100%\" height=\"%2\">"
                               "<line x1=\"%3\" y1=\"%4\" x2=\"%5\" y2=\"%4\" stroke=\"var(--border)\" stroke-width=\"1\"/>"
                               "<line x1=\"%3\" y1=\"%6\" x2=\"%5\" y2=\"%6\" stroke=\"var(--border)\" stroke-width=\"1\"/>"
                               "<text x=\"4\" y=\"%4\">100%</text><text x=\"4\" y=\"%6\">0%</text>"
                               "<polyline fill=\"none\" stroke=\"var(--accent)\" stroke-width=\"2\" points=\"%7\"/>"
                               "<polyline fill=\"none\" stroke=\"var(--warning)\" stroke-width=\"2\" points=\"%8\"/>"
                               "<polyline fill=\"none\" stroke=\"var(--positive)\" stroke-width=\"2\" points=\"%9\"/>"
                               "</svg></div>"
                               "<div style=\"display:flex;gap:18px;margin-top:12px;font-size:13px;\">"
                               "<div><span style=\"color:var(--accent);font-weight:bold;\">―</span> CPU Load (%)</div>"
                               "<div><span style=\"color:var(--warning);font-weight:bold;\">―</span> CPU Temp (°C)</div>"
                               "<div><span style=\"color:var(--positive);font-weight:bold;\">―</span> Responsiveness (Score/100)</div>"
                               "</div></div>\n")
                    .arg(w).arg(h).arg(pad).arg(pad).arg(w - pad).arg(pad + plotH)
                    .arg(cpuPoints.trimmed(), tempPoints.trimmed(), respPoints.trimmed());
    }

    if (!comparison.isEmpty()) {
        html += QStringLiteral("<div class=\"card\"><h2>Incident Comparison Analysis</h2><table>"
                               "<thead><tr><th>Period</th><th>Samples</th><th>CPU Avg</th><th>Responsiveness</th><th>Coverage</th></tr></thead><tbody>");
        for (const auto &period : {QStringLiteral("before"), QStringLiteral("during"), QStringLiteral("after")}) {
            const auto p = comparison.value(period).toMap();
            if (!p.isEmpty()) {
                html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td><td>%3%</td><td>%4/100</td><td>%5 ms</td></tr>")
                    .arg(period)
                    .arg(p.value(QStringLiteral("sampleCount")).toInt())
                    .arg(p.value(QStringLiteral("cpuLoadPercent")).toDouble(), 0, 'f', 1)
                    .arg(p.value(QStringLiteral("responsivenessScore")).toDouble(), 0, 'f', 1)
                    .arg(p.value(QStringLiteral("coverageMs")).toLongLong());
            }
        }
        html += QStringLiteral("</tbody></table></div>\n");
    }

    if (!kernelLogs.isEmpty()) {
        html += QStringLiteral("<div class=\"card\"><h2>Correlated Kernel Events (±15s window, privacy-redacted)</h2><table>"
                               "<thead><tr><th>Time</th><th>Subsystem</th><th>Priority</th><th>Message</th></tr></thead><tbody>");
        for (const auto &entryVal : kernelLogs) {
            const auto entry = entryVal.toMap();
            const QString timeStr = QDateTime::fromMSecsSinceEpoch(entry.value(QStringLiteral("timestampMs")).toLongLong()).toString(QStringLiteral("HH:mm:ss"));
            html += QStringLiteral("<tr><td>%1</td><td><code>%2</code></td><td>%3</td><td>%4</td></tr>")
                .arg(timeStr)
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
    const QVariantList kernelLogs = incidentKernelLogs(QString());
    QPointer<PulseTelemetryController> guard(this);
    m_actionPending = true; Q_EMIT telemetryUpdated();
    m_filePool.start(QRunnable::create([guard, recorder, selection = safeSelection, comparison = safeComparison, filters, base, include, kernelLogs] {
        const QString directory = QDir(base).filePath(QStringLiteral("reports"));
        QString status, saved;
        bool succeeded = false;
        if (base.isEmpty() || (!QDir().mkpath(directory) && !QDir(directory).exists())) status = QStringLiteral("The reports folder could not be created.");
        else {
            const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
            const QString htmlPath = QDir(directory).filePath(QStringLiteral("loofi-pulse-report-%1.html").arg(stamp));
            const QString markdownPath = QDir(directory).filePath(QStringLiteral("loofi-pulse-report-%1.md").arg(stamp));
            const QString jsonPath = QDir(directory).filePath(QStringLiteral("loofi-pulse-samples-%1.json").arg(stamp));
            auto root = QJsonDocument::fromJson(recorder.exportSamplesJson(include).toUtf8()).array();
            for (int i = 0; i < root.size(); ++i) {
                auto sample = root.at(i).toObject();
                sample.insert(QStringLiteral("selection"), QJsonObject::fromVariantMap(selection));
                sample.insert(QStringLiteral("viewSelection"), QJsonObject::fromVariantMap(filters));
                if (i == 0 && !kernelLogs.isEmpty()) {
                    sample.insert(QStringLiteral("correlatedKernelEvents"), QJsonArray::fromVariantList(kernelLogs));
                }
                root[i] = sample;
            }
            QString markdown = recorder.exportSummaryMarkdown(include);
            if (!selection.isEmpty()) markdown += QStringLiteral("\n## Selected investigation\n\nOriginal selection sample IDs: %1–%2. The report includes available before/after context; missing context is not inferred.\n")
                .arg(selection.value(QStringLiteral("selectionStartSampleId")).toString(), selection.value(QStringLiteral("selectionEndSampleId")).toString());

            const auto comp = selection.value(QStringLiteral("comparison")).toMap();
            if (!comp.isEmpty()) {
                markdown += QStringLiteral("\n## Incident Comparison Analysis\n\n");
                for (const auto &period : {QStringLiteral("before"), QStringLiteral("during"), QStringLiteral("after")}) {
                    const auto p = comp.value(period).toMap();
                    if (!p.isEmpty()) {
                        markdown += QStringLiteral("- **%1**: %2 samples; CPU avg: %3%; Responsiveness: %4/100; Coverage: %5 ms\n")
                            .arg(period)
                            .arg(p.value(QStringLiteral("sampleCount")).toInt())
                            .arg(p.value(QStringLiteral("cpuLoadPercent")).toDouble(), 0, 'f', 1)
                            .arg(p.value(QStringLiteral("responsivenessScore")).toDouble(), 0, 'f', 1)
                            .arg(p.value(QStringLiteral("coverageMs")).toLongLong());
                    }
                }
            }

            if (!kernelLogs.isEmpty()) {
                markdown += QStringLiteral("\n## Correlated Kernel Events (±15s window, privacy-redacted)\n\n");
                for (const auto &entryVal : kernelLogs) {
                    const auto entry = entryVal.toMap();
                    const QString timeStr = QDateTime::fromMSecsSinceEpoch(entry.value(QStringLiteral("timestampMs")).toLongLong()).toString(QStringLiteral("HH:mm:ss"));
                    markdown += QStringLiteral("- [%1] [%2] %3 (priority: %4)\n")
                        .arg(timeStr)
                        .arg(entry.value(QStringLiteral("subsystem")).toString().toUpper())
                        .arg(entry.value(QStringLiteral("message")).toString())
                        .arg(entry.value(QStringLiteral("severity")).toString());
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
