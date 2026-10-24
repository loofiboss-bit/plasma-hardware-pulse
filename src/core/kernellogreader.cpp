#include "kernellogreader.h"

#include <QDateTime>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

namespace LoofiPulse
{

QString KernelLogReader::redactSensitiveData(const QString &text)
{
    QString result = text;

    // Redact MAC addresses
    static const QRegularExpression macRe(QStringLiteral("\\b[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}\\b"));
    result.replace(macRe, QStringLiteral("[mac-redacted]"));

    // Redact IPv4 addresses
    static const QRegularExpression ipv4Re(QStringLiteral("\\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b"));
    result.replace(ipv4Re, QStringLiteral("[ip-redacted]"));

    // Redact user home paths
    static const QRegularExpression homeRe(QStringLiteral("/home/[a-zA-Z0-9_.-]+"));
    result.replace(homeRe, QStringLiteral("/home/[user]"));

    return result;
}

bool KernelLogReader::isHardwareRelevant(const QString &text)
{
    static const QRegularExpression hwRe(
        QStringLiteral("(?i)(thermal|throttle|prochot|cpu|gpu|drm|i915|amdgpu|nouveau|nvidia|nvme|ata|scsi|sata|disk|block|oom-killer|out of memory|mce|power|battery|acpi|edac|hardware error|overheat|fan|volt)"));
    return hwRe.match(text).hasMatch();
}

QList<KernelLogEntry> KernelLogReader::parseJournalLines(const QString &rawOutput)
{
    QList<KernelLogEntry> entries;
    const QStringList lines = rawOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    // Matches: 2026-10-24T21:38:40+02:00 host kernel: message
    static const QRegularExpression lineRe(
        QStringLiteral("^(\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}[+-]\\d{2}:?\\d{2})\\s+\\S+\\s+kernel:\\s+(.*)$"));

    for (const QString &rawLine : lines) {
        const auto match = lineRe.match(rawLine);
        QString timeStr;
        QString msg;

        if (match.hasMatch()) {
            timeStr = match.captured(1);
            msg = match.captured(2).trimmed();
        } else {
            // Fallback for syslog or raw formats
            const int kernelIdx = rawLine.indexOf(QStringLiteral("kernel: "));
            if (kernelIdx >= 0) {
                timeStr = rawLine.left(kernelIdx).trimmed();
                msg = rawLine.mid(kernelIdx + 8).trimmed();
            } else {
                msg = rawLine.trimmed();
            }
        }

        if (msg.isEmpty()) continue;
        if (!isHardwareRelevant(msg)) continue;

        KernelLogEntry entry;
        entry.message = redactSensitiveData(msg);

        const QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
        if (dt.isValid()) {
            entry.epochMs = dt.toMSecsSinceEpoch();
            entry.timestamp = dt.toString(QStringLiteral("HH:mm:ss"));
        } else {
            entry.timestamp = timeStr;
        }

        // Subsystem detection
        const QString lower = msg.toLower();
        if (lower.contains(QStringLiteral("thermal")) || lower.contains(QStringLiteral("throttle")) || lower.contains(QStringLiteral("overheat")))
            entry.subsystem = QStringLiteral("Thermal");
        else if (lower.contains(QStringLiteral("gpu")) || lower.contains(QStringLiteral("drm")) || lower.contains(QStringLiteral("amdgpu")) || lower.contains(QStringLiteral("i915")) || lower.contains(QStringLiteral("nvidia")))
            entry.subsystem = QStringLiteral("Graphics");
        else if (lower.contains(QStringLiteral("nvme")) || lower.contains(QStringLiteral("ata")) || lower.contains(QStringLiteral("disk")) || lower.contains(QStringLiteral("scsi")))
            entry.subsystem = QStringLiteral("Storage");
        else if (lower.contains(QStringLiteral("oom")) || lower.contains(QStringLiteral("out of memory")) || lower.contains(QStringLiteral("kill process")))
            entry.subsystem = QStringLiteral("Memory/OOM");
        else if (lower.contains(QStringLiteral("power")) || lower.contains(QStringLiteral("battery")) || lower.contains(QStringLiteral("acpi")))
            entry.subsystem = QStringLiteral("Power");
        else if (lower.contains(QStringLiteral("cpu")) || lower.contains(QStringLiteral("mce")) || lower.contains(QStringLiteral("edac")))
            entry.subsystem = QStringLiteral("CPU/Hardware");
        else
            entry.subsystem = QStringLiteral("Kernel");

        // Severity detection
        if (lower.contains(QStringLiteral("error")) || lower.contains(QStringLiteral("crit")) || lower.contains(QStringLiteral("fail")) || lower.contains(QStringLiteral("oom")) || lower.contains(QStringLiteral("throttl")))
            entry.severity = QStringLiteral("error");
        else if (lower.contains(QStringLiteral("warn")) || lower.contains(QStringLiteral("limit")) || lower.contains(QStringLiteral("high")))
            entry.severity = QStringLiteral("warning");
        else
            entry.severity = QStringLiteral("info");

        entries.append(entry);
    }

    return entries;
}

QList<KernelLogEntry> KernelLogReader::fetchIncidentLogs(qint64 startMs, qint64 endMs, qint64 marginMs, const QString &mockOutput)
{
    if (!mockOutput.isEmpty()) {
        return parseJournalLines(mockOutput);
    }

    if (startMs <= 0) return {};
    const qint64 queryStart = std::max<qint64>(0, startMs - marginMs);
    const qint64 queryEnd = (endMs > 0 && endMs >= startMs) ? (endMs + marginMs) : (startMs + marginMs);

    const QString sinceStr = QDateTime::fromMSecsSinceEpoch(queryStart).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString untilStr = QDateTime::fromMSecsSinceEpoch(queryEnd).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    QProcess process;
    const QStringList args = {
        QStringLiteral("-k"),
        QStringLiteral("--no-pager"),
        QStringLiteral("--output=short-iso"),
        QStringLiteral("--since"), sinceStr,
        QStringLiteral("--until"), untilStr
    };

    process.start(QStringLiteral("journalctl"), args);
    if (!process.waitForFinished(3000)) {
        process.kill();
        return {};
    }

    if (process.exitCode() != 0) {
        return {};
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    return parseJournalLines(output);
}

QVariantList KernelLogReader::fetchIncidentLogsVariant(qint64 startMs, qint64 endMs, qint64 marginMs, const QString &mockOutput)
{
    const QList<KernelLogEntry> entries = fetchIncidentLogs(startMs, endMs, marginMs, mockOutput);
    QVariantList list;
    list.reserve(entries.size());
    for (const auto &entry : entries) {
        QVariantMap map;
        map.insert(QStringLiteral("timestamp"), entry.timestamp);
        map.insert(QStringLiteral("epochMs"), entry.epochMs);
        map.insert(QStringLiteral("subsystem"), entry.subsystem);
        map.insert(QStringLiteral("message"), entry.message);
        map.insert(QStringLiteral("severity"), entry.severity);
        list.append(map);
    }
    return list;
}

} // namespace LoofiPulse
