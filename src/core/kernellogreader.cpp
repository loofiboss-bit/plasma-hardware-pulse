#include "kernellogreader.h"

#include <arpa/inet.h>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include <limits>

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

    // Validate IPv6 candidates so PCI BDFs such as 0000:03:00.0 remain intact.
    static const QRegularExpression ipv6Re(
        QStringLiteral("(?<![0-9a-fA-F:.])([0-9a-fA-F:]*:[0-9a-fA-F:]*)(?![0-9a-fA-F:]|\\.\\d)"));
    auto ipv6Match = ipv6Re.globalMatch(result);
    QString redacted;
    qsizetype copiedUntil = 0;
    while (ipv6Match.hasNext()) {
        const auto match = ipv6Match.next();
        const QByteArray candidate = match.captured(1).toLatin1();
        in6_addr address{};
        if (inet_pton(AF_INET6, candidate.constData(), &address) != 1) continue;
        redacted += result.mid(copiedUntil, match.capturedStart(1) - copiedUntil);
        redacted += QStringLiteral("[ip-redacted]");
        copiedUntil = match.capturedEnd(1);
    }
    if (copiedUntil > 0) redacted += result.mid(copiedUntil);
    if (!redacted.isEmpty()) result = redacted;

    // Redact user home paths
    static const QRegularExpression homeRe(QStringLiteral("/home/[a-zA-Z0-9_.-]+"));
    result.replace(homeRe, QStringLiteral("/home/[user]"));

    // OOM diagnostics can include process IDs and command names unrelated to the hardware event.
    static const QRegularExpression processRe(QStringLiteral("(?i)\\b(killed|kill(?:ing)?)\\s+process\\s+\\d+(?:\\s+\\([^)]*\\))?"));
    result.replace(processRe, QStringLiteral("\\1 process [redacted]"));

    if (result.contains(QStringLiteral("oom-killer"), Qt::CaseInsensitive)
        || result.contains(QStringLiteral("oom-kill"), Qt::CaseInsensitive)) {
        static const QRegularExpression invokedRe(QStringLiteral("(?i)(^|\\s)([^\\s:]+)\\s+invoked\\s+oom-killer\\b"));
        result.replace(invokedRe, QStringLiteral("\\1[process-redacted] invoked oom-killer"));
        for (const QString &field : {QStringLiteral("task"), QStringLiteral("comm")}) {
            const QRegularExpression fieldRe(QStringLiteral("(?i)(\\b%1=)[^,\\s]+").arg(field));
            result.replace(fieldRe, QStringLiteral("\\1[redacted]"));
        }
        static const QRegularExpression oomPidRe(QStringLiteral("(?i)(\\bpid=)\\d+"));
        result.replace(oomPidRe, QStringLiteral("\\1[redacted]"));
    }

    return result;
}

bool KernelLogReader::isHardwareRelevant(const QString &text)
{
    static const QRegularExpression hwRe(
        QStringLiteral("(?i)(thermal|throttle|prochot|cpu|gpu|drm|i915|amdgpu|nouveau|nvidia|nvme|ata|scsi|sata|disk|block|oom-kill(?:er)?|out of memory|mce|power|battery|acpi|edac|hardware error|overheat|fan|volt)"));
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
        const QJsonDocument json = rawLine.startsWith(QLatin1Char('{'))
            ? QJsonDocument::fromJson(rawLine.toUtf8()) : QJsonDocument{};
        if (json.isObject() && json.object().contains(QStringLiteral("MESSAGE"))) {
            const QJsonObject object = json.object();
            const QString message = object.value(QStringLiteral("MESSAGE")).toString().trimmed();
            if (message.isEmpty() || !isHardwareRelevant(message)) continue;
            KernelLogEntry entry;
            entry.message = redactSensitiveData(message);
            bool timeOk = false;
            const quint64 timestampUs = object.value(QStringLiteral("__REALTIME_TIMESTAMP")).toString().toULongLong(&timeOk);
            if (timeOk && timestampUs / 1000ULL <= static_cast<quint64>(std::numeric_limits<qint64>::max())) {
                entry.epochMs = static_cast<qint64>(timestampUs / 1000ULL);
                entry.timestamp = QDateTime::fromMSecsSinceEpoch(entry.epochMs).toLocalTime().toString(QStringLiteral("HH:mm:ss"));
            }
            const QString lower = message.toLower();
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

            bool priorityOk = false;
            const int priority = object.value(QStringLiteral("PRIORITY")).toString().toInt(&priorityOk);
            if (priorityOk && priority <= 3) entry.severity = QStringLiteral("error");
            else if (priorityOk && priority == 4) entry.severity = QStringLiteral("warning");
            else if (priorityOk) entry.severity = QStringLiteral("info");
            else if (lower.contains(QStringLiteral("error")) || lower.contains(QStringLiteral("crit")) || lower.contains(QStringLiteral("fail")) || lower.contains(QStringLiteral("oom")) || lower.contains(QStringLiteral("throttl")))
                entry.severity = QStringLiteral("error");
            else if (lower.contains(QStringLiteral("warn")) || lower.contains(QStringLiteral("limit")) || lower.contains(QStringLiteral("high")))
                entry.severity = QStringLiteral("warning");
            else
                entry.severity = QStringLiteral("info");
            entries.append(entry);
            continue;
        }

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

        QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODateWithMs);
        if (!dt.isValid()) dt = QDateTime::fromString(timeStr, Qt::ISODate);
        if (dt.isValid()) {
            entry.epochMs = dt.toMSecsSinceEpoch();
            entry.timestamp = dt.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
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

    const auto journalTime = [](qint64 timeMs) {
        return QStringLiteral("@%1.%2").arg(timeMs / 1000).arg(timeMs % 1000, 3, 10, QLatin1Char('0'));
    };

    QProcess process;
    const QStringList args = {
        QStringLiteral("--no-pager"),
        QStringLiteral("--quiet"),
        QStringLiteral("--output=json"),
        QStringLiteral("_TRANSPORT=kernel"),
        QStringLiteral("--since"), journalTime(queryStart),
        QStringLiteral("--until"), journalTime(queryEnd),
        QStringLiteral("--lines=1000")
    };

    process.start(QStringLiteral("journalctl"), args);
    if (!process.waitForStarted(1000) || !process.waitForFinished(3000)) {
        process.kill();
        process.waitForFinished(1000);
        return {};
    }

    if (process.exitCode() != 0) {
        return {};
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const qint64 allowedStart = queryStart;
    const qint64 allowedEnd = queryEnd;
    QList<KernelLogEntry> entries = parseJournalLines(output);
    entries.erase(std::remove_if(entries.begin(), entries.end(), [allowedStart, allowedEnd](const KernelLogEntry &entry) {
        return entry.epochMs > 0 && (entry.epochMs < allowedStart || entry.epochMs > allowedEnd);
    }), entries.end());
    return entries;
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
        map.insert(QStringLiteral("timestampMs"), entry.epochMs);
        map.insert(QStringLiteral("subsystem"), entry.subsystem);
        map.insert(QStringLiteral("message"), entry.message);
        map.insert(QStringLiteral("severity"), entry.severity);
        list.append(map);
    }
    return list;
}

} // namespace LoofiPulse
