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
    m_filePool.start(QRunnable::create([guard, recorder, selection = safeSelection, filters, base, include, kernelLogs] {
        const QString directory = QDir(base).filePath(QStringLiteral("reports"));
        QString status, saved;
        bool succeeded = false;
        if (base.isEmpty() || (!QDir().mkpath(directory) && !QDir(directory).exists())) status = QStringLiteral("The reports folder could not be created.");
        else {
            const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
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
            const auto write = [](const QString &path, const QByteArray &bytes, QString &error) {
                QSaveFile file(path);
                if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { error = file.errorString(); return false; }
                return true;
            };
            QString markdownError, jsonError;
            const bool md = write(markdownPath, markdown.toUtf8(), markdownError);
            const bool js = write(jsonPath, QJsonDocument(root).toJson(QJsonDocument::Indented), jsonError);
            succeeded = md && js;
            saved = md ? markdownPath : js ? jsonPath : QString();
            if (succeeded) status = QStringLiteral("Saved Markdown and JSON reports.");
            else if (md) status = QStringLiteral("Markdown saved at %1; JSON failed: %2").arg(markdownPath, jsonError);
            else if (js) status = QStringLiteral("JSON saved at %1; Markdown failed: %2").arg(jsonPath, markdownError);
            else status = QStringLiteral("Report export failed. Markdown: %1. JSON: %2").arg(markdownError, jsonError);
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
