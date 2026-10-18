#include "sessionstore.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QRegularExpression>
#include <cmath>
#include <limits>
namespace LoofiPulse {
namespace {
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
bool integerValue(const QVariant &v, bool unsignedValue) {
    if (v.metaType().id() == QMetaType::QString) {
        bool ok = false;
        if (unsignedValue) v.toString().toULongLong(&ok); else v.toString().toLongLong(&ok);
        return ok && !v.toString().isEmpty() && !v.toString().startsWith(QLatin1Char('-'));
    }
    if (v.metaType().id() != QMetaType::Double && v.metaType().id() != QMetaType::Int
        && v.metaType().id() != QMetaType::UInt && v.metaType().id() != QMetaType::LongLong
        && v.metaType().id() != QMetaType::ULongLong) return false;
    const double n = v.toDouble();
    return std::isfinite(n) && n >= 0 && n <= 9007199254740991.0 && std::floor(n) == n;
}
}
namespace {
void collectProcessNames(const QVariant &value, QSet<QString> &names, QSet<QString> &pids, bool processContext = false)
{
    if (value.metaType().id() == QMetaType::QVariantMap) {
        const auto map = value.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            const QString key = it.key().toLower();
            const bool context = processContext || key.contains(QStringLiteral("process")) || key.contains(QStringLiteral("culprit"));
            if (context && (key.contains(QStringLiteral("name")) || key.contains(QStringLiteral("command"))
                || key.contains(QStringLiteral("executable")) || key.contains(QStringLiteral("identity")))) {
                if (it.value().metaType().id() == QMetaType::QString && !it.value().toString().isEmpty()) names.insert(it.value().toString());
            }
            if (key == QStringLiteral("pid") || key == QStringLiteral("culpritpid") || key == QStringLiteral("ppid")) {
                if (it.value().toLongLong() > 0) pids.insert(it.value().toString());
            }
            collectProcessNames(it.value(), names, pids, context);
        }
    } else if (value.metaType().id() == QMetaType::QVariantList) {
        for (const auto &item : value.toList()) collectProcessNames(item, names, pids, processContext);
    }
}
QVariant redactValue(const QVariant &value, const QSet<QString> &names, const QSet<QString> &pids, bool removeIdentityFields = true)
{
    if (value.metaType().id() == QMetaType::QVariantMap) {
        QVariantMap result;
        const auto map = value.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            const QString key = it.key().toLower();
            if (removeIdentityFields && ((key.contains(QStringLiteral("process")) && key != QStringLiteral("processdetailsincluded")) || key.contains(QStringLiteral("culprit"))
                || key == QStringLiteral("pid") || key == QStringLiteral("ppid")
                || key.contains(QStringLiteral("command")) || key.contains(QStringLiteral("executable"))
                || key.startsWith(QStringLiteral("owner")) || key == QStringLiteral("username")
                || key.contains(QStringLiteral("starttime")))) continue;
            result.insert(it.key(), redactValue(it.value(), names, pids, removeIdentityFields));
        }
        return result;
    }
    if (value.metaType().id() == QMetaType::QVariantList) {
        QVariantList result;
        for (const auto &item : value.toList()) result.append(redactValue(item, names, pids, removeIdentityFields));
        return result;
    }
    if (value.metaType().id() == QMetaType::QStringList) {
        QStringList result;
        for (const auto &item : value.toStringList()) result.append(redactValue(item, names, pids, removeIdentityFields).toString());
        return result;
    }
    if (value.metaType().id() == QMetaType::QString) {
        QString text = value.toString();
        for (const auto &name : names) text.replace(QRegularExpression(QStringLiteral("(?<![\\w])%1(?![\\w])").arg(QRegularExpression::escape(name))), QStringLiteral("[process]"));
        for (const auto &pid : pids) text.replace(QRegularExpression(QStringLiteral("\\bPID[ :=#]*%1\\b").arg(QRegularExpression::escape(pid)), QRegularExpression::CaseInsensitiveOption), QStringLiteral("PID [redacted]"));
        return text;
    }
    return value;
}
}
QVariant SessionStore::redactProcessDetails(const QVariant &value)
{
    QSet<QString> names, pids;
    collectProcessNames(value, names, pids);
    return redactValue(value, names, pids);
}
QVariant SessionStore::redactReportText(const QVariant &value, const QList<FlightSample> &recording)
{
    QSet<QString> names, pids;
    for (const auto &sample : recording) collectProcessNames(sampleToMap(sample, true), names, pids);
    // The report has already removed identities. Preserve its schema and null fields.
    return redactValue(value, names, pids, false);
}
QVariantMap SessionStore::sampleToMap(const FlightSample &s, bool includeProcessDetails) {
    QVariantMap map;
    map.insert(QStringLiteral("contributingSignals"), QVariant::fromValue(s.contributingSignals));
    map.insert(QStringLiteral("sampleId"), QString::number(s.sampleId));
    map.insert(QStringLiteral("timestampMs"), QString::number(s.timestampMs));
    map.insert(QStringLiteral("monotonicMs"), QString::number(s.monotonicMs));
    map.insert(QStringLiteral("elapsedSincePreviousMs"), QString::number(s.elapsedSincePreviousMs));
    map.insert(QStringLiteral("cpuTempCelsius"), QVariant::fromValue(s.cpuTempCelsius));
    map.insert(QStringLiteral("nvmeTempCelsius"), QVariant::fromValue(s.nvmeTempCelsius));
    map.insert(QStringLiteral("wifiTempCelsius"), QVariant::fromValue(s.wifiTempCelsius));
    map.insert(QStringLiteral("cpuLoadPercent"), QVariant::fromValue(s.cpuLoadPercent));
    map.insert(QStringLiteral("cpuFreqMhz"), QVariant::fromValue(s.cpuFreqMhz));
    map.insert(QStringLiteral("gpuBusyPercent"), QVariant::fromValue(s.gpuBusyPercent));
    map.insert(QStringLiteral("gpuTempCelsius"), QVariant::fromValue(s.gpuTempCelsius));
    map.insert(QStringLiteral("gpuFreqMhz"), QVariant::fromValue(s.gpuFreqMhz));
    map.insert(QStringLiteral("gpuPowerWatts"), QVariant::fromValue(s.gpuPowerWatts));
    map.insert(QStringLiteral("gpuVramUsedMegabytes"), QVariant::fromValue(s.gpuVramUsedMegabytes));
    map.insert(QStringLiteral("gpuVramTotalMegabytes"), QVariant::fromValue(s.gpuVramTotalMegabytes));
    map.insert(QStringLiteral("psiCpuSome10"), QVariant::fromValue(s.psiCpuSome10));
    map.insert(QStringLiteral("psiCpuSome60"), QVariant::fromValue(s.psiCpuSome60));
    map.insert(QStringLiteral("psiMemSome10"), QVariant::fromValue(s.psiMemSome10));
    map.insert(QStringLiteral("psiMemSome60"), QVariant::fromValue(s.psiMemSome60));
    map.insert(QStringLiteral("psiMemFull10"), QVariant::fromValue(s.psiMemFull10));
    map.insert(QStringLiteral("psiMemFull60"), QVariant::fromValue(s.psiMemFull60));
    map.insert(QStringLiteral("psiIoSome10"), QVariant::fromValue(s.psiIoSome10));
    map.insert(QStringLiteral("psiIoSome60"), QVariant::fromValue(s.psiIoSome60));
    map.insert(QStringLiteral("psiIoFull10"), QVariant::fromValue(s.psiIoFull10));
    map.insert(QStringLiteral("psiIoFull60"), QVariant::fromValue(s.psiIoFull60));
    map.insert(QStringLiteral("psiCpuStallMsPerSec"), QVariant::fromValue(s.psiCpuStallMsPerSec));
    map.insert(QStringLiteral("psiMemStallMsPerSec"), QVariant::fromValue(s.psiMemStallMsPerSec));
    map.insert(QStringLiteral("psiIoStallMsPerSec"), QVariant::fromValue(s.psiIoStallMsPerSec));
    map.insert(QStringLiteral("responsivenessScore"), QVariant::fromValue(s.responsivenessScore));
    map.insert(QStringLiteral("systemWatts"), QVariant::fromValue(s.systemWatts));
    map.insert(QStringLiteral("packageWatts"), QVariant::fromValue(s.packageWatts));
    map.insert(QStringLiteral("batteryWatts"), QVariant::fromValue(s.batteryWatts));
    map.insert(QStringLiteral("batteryPercent"), QVariant::fromValue(s.batteryPercent));
    map.insert(QStringLiteral("batteryHealthPercent"), QVariant::fromValue(s.batteryHealthPercent));
    map.insert(QStringLiteral("pl1LimitWatts"), QVariant::fromValue(s.pl1LimitWatts));
    map.insert(QStringLiteral("pl2LimitWatts"), QVariant::fromValue(s.pl2LimitWatts));
    map.insert(QStringLiteral("fanRpm"), QVariant::fromValue(s.fanRpm));
    map.insert(QStringLiteral("ramUsedPercent"), QVariant::fromValue(s.ramUsedPercent));
    map.insert(QStringLiteral("ramUsedBytes"), QString::number(s.ramUsedBytes));
    map.insert(QStringLiteral("ramTotalBytes"), QString::number(s.ramTotalBytes));
    map.insert(QStringLiteral("swapUsedBytes"), QString::number(s.swapUsedBytes));
    map.insert(QStringLiteral("swapTotalBytes"), QString::number(s.swapTotalBytes));
    map.insert(QStringLiteral("zramCompressionRatio"), QVariant::fromValue(s.zramCompressionRatio));
    map.insert(QStringLiteral("zramOriginalBytes"), QString::number(s.zramOriginalBytes));
    map.insert(QStringLiteral("zramCompressedBytes"), QString::number(s.zramCompressedBytes));
    map.insert(QStringLiteral("zramMemoryUsedBytes"), QString::number(s.zramMemoryUsedBytes));
    map.insert(QStringLiteral("diskReadMibPerSec"), QVariant::fromValue(s.diskReadMibPerSec));
    map.insert(QStringLiteral("diskWriteMibPerSec"), QVariant::fromValue(s.diskWriteMibPerSec));
    map.insert(QStringLiteral("netRxMibPerSec"), QVariant::fromValue(s.netRxMibPerSec));
    map.insert(QStringLiteral("netTxMibPerSec"), QVariant::fromValue(s.netTxMibPerSec));
    map.insert(QStringLiteral("throttleEventCount"), QString::number(s.throttleEventCount));
    map.insert(QStringLiteral("newThrottleEvents"), QString::number(s.newThrottleEvents));
    map.insert(QStringLiteral("batteryPresent"), QVariant::fromValue(s.batteryPresent));
    map.insert(QStringLiteral("onAcPower"), QVariant::fromValue(s.onAcPower));
    map.insert(QStringLiteral("activePowerProfile"), QVariant::fromValue(s.activePowerProfile));
    map.insert(QStringLiteral("throttled"), QVariant::fromValue(s.throttled));
    map.insert(QStringLiteral("severity"), static_cast<int>(s.severity));
    map.insert(QStringLiteral("verdictHeadline"), QVariant::fromValue(s.verdictHeadline));
    map.insert(QStringLiteral("verdictRuleId"), QVariant::fromValue(s.verdictRuleId));
    map.insert(QStringLiteral("verdictEvidence"), QVariant::fromValue(s.verdictEvidence));
    map.insert(QStringLiteral("verdictDurationMs"), QString::number(s.verdictDurationMs));
    map.insert(QStringLiteral("culpritName"), QVariant::fromValue(s.culpritName));
    map.insert(QStringLiteral("culpritPid"), QVariant::fromValue(s.culpritPid));
    map.insert(QStringLiteral("culpritStartTimeTicks"), QString::number(s.culpritStartTimeTicks));
    map.insert(QStringLiteral("processObservedAtMs"), QString::number(s.processObservedAtMs));
    map.insert(QStringLiteral("culpritCpuPercent"), QVariant::fromValue(s.culpritCpuPercent));
    map.insert(QStringLiteral("culpritRssMb"), QVariant::fromValue(s.culpritRssMb));
    map.insert(QStringLiteral("metricQuality"), QVariant::fromValue(s.metricQuality));
    map.insert(QStringLiteral("gpuDevices"), QVariant::fromValue(s.gpuDevices));
    map.insert(QStringLiteral("storageDevices"), QVariant::fromValue(s.storageDevices));
    map.insert(QStringLiteral("networkDevices"), QVariant::fromValue(s.networkDevices));
    map.insert(QStringLiteral("batteries"), QVariant::fromValue(s.batteries));
    map.insert(QStringLiteral("processObservations"), QVariant::fromValue(s.processObservations));
    map.insert(QStringLiteral("captureId"), QVariant::fromValue(s.captureId));
    map.insert(QStringLiteral("engineVersion"), QVariant::fromValue(s.engineVersion));
    map.insert(QStringLiteral("verdictRecommendation"), QVariant::fromValue(s.verdictRecommendation));
    map.insert(QStringLiteral("verdictCausalChain"), QVariant::fromValue(s.verdictCausalChain));
    if (!includeProcessDetails) {
        map = redactProcessDetails(map).toMap();
    }
    return map;
}
bool SessionStore::sampleFromMap(const QVariantMap &map, FlightSample *sample, QString *error) {
    if (!sample) return fail(error, QStringLiteral("No sample destination was provided."));
    FlightSample s;
    if (!map.contains(QStringLiteral("sampleId")) || !map.contains(QStringLiteral("timestampMs")))
        return fail(error, QStringLiteral("A sample is missing its identity or timestamp."));
    if (map.contains(QStringLiteral("contributingSignals"))) {
        const QVariant v = map.value(QStringLiteral("contributingSignals"));
        if (!(v.metaType().id() == QMetaType::QStringList || v.metaType().id() == QMetaType::QVariantList)) return fail(error, QStringLiteral("Invalid sample field: contributingSignals."));
        for (const auto &item : v.toList()) if (item.metaType().id() != QMetaType::QString) return fail(error, QStringLiteral("Invalid contributing signals."));
        s.contributingSignals = v.toStringList();
    }
    if (map.contains(QStringLiteral("sampleId"))) {
        const QVariant v = map.value(QStringLiteral("sampleId"));
        if (!(integerValue(v, true))) return fail(error, QStringLiteral("Invalid sample field: sampleId."));
        s.sampleId = v.toULongLong();
    }
    if (map.contains(QStringLiteral("timestampMs"))) {
        const QVariant v = map.value(QStringLiteral("timestampMs"));
        if (!(integerValue(v, false))) return fail(error, QStringLiteral("Invalid sample field: timestampMs."));
        s.timestampMs = v.toLongLong();
    }
    if (map.contains(QStringLiteral("monotonicMs"))) {
        const QVariant v = map.value(QStringLiteral("monotonicMs"));
        if (!(integerValue(v, false))) return fail(error, QStringLiteral("Invalid sample field: monotonicMs."));
        s.monotonicMs = v.toLongLong();
    }
    if (map.contains(QStringLiteral("elapsedSincePreviousMs"))) {
        const QVariant v = map.value(QStringLiteral("elapsedSincePreviousMs"));
        if (!(integerValue(v, false))) return fail(error, QStringLiteral("Invalid sample field: elapsedSincePreviousMs."));
        s.elapsedSincePreviousMs = v.toLongLong();
    }
    if (map.contains(QStringLiteral("cpuTempCelsius"))) {
        const QVariant v = map.value(QStringLiteral("cpuTempCelsius"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: cpuTempCelsius."));
        s.cpuTempCelsius = v.toDouble();
    }
    if (map.contains(QStringLiteral("nvmeTempCelsius"))) {
        const QVariant v = map.value(QStringLiteral("nvmeTempCelsius"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: nvmeTempCelsius."));
        s.nvmeTempCelsius = v.toDouble();
    }
    if (map.contains(QStringLiteral("wifiTempCelsius"))) {
        const QVariant v = map.value(QStringLiteral("wifiTempCelsius"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: wifiTempCelsius."));
        s.wifiTempCelsius = v.toDouble();
    }
    if (map.contains(QStringLiteral("cpuLoadPercent"))) {
        const QVariant v = map.value(QStringLiteral("cpuLoadPercent"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: cpuLoadPercent."));
        s.cpuLoadPercent = v.toDouble();
    }
    if (map.contains(QStringLiteral("cpuFreqMhz"))) {
        const QVariant v = map.value(QStringLiteral("cpuFreqMhz"));
        if (!(integerValue(v, false) && v.toLongLong() <= std::numeric_limits<int>::max())) return fail(error, QStringLiteral("Invalid sample field: cpuFreqMhz."));
        s.cpuFreqMhz = v.toInt();
    }
    if (map.contains(QStringLiteral("gpuBusyPercent"))) {
        const QVariant v = map.value(QStringLiteral("gpuBusyPercent"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: gpuBusyPercent."));
        s.gpuBusyPercent = v.toDouble();
    }
    if (map.contains(QStringLiteral("gpuTempCelsius"))) {
        const QVariant v = map.value(QStringLiteral("gpuTempCelsius"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: gpuTempCelsius."));
        s.gpuTempCelsius = v.toDouble();
    }
    if (map.contains(QStringLiteral("gpuFreqMhz"))) {
        const QVariant v = map.value(QStringLiteral("gpuFreqMhz"));
        if (!(integerValue(v, false) && v.toLongLong() <= std::numeric_limits<int>::max())) return fail(error, QStringLiteral("Invalid sample field: gpuFreqMhz."));
        s.gpuFreqMhz = v.toInt();
    }
    if (map.contains(QStringLiteral("gpuPowerWatts"))) {
        const QVariant v = map.value(QStringLiteral("gpuPowerWatts"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: gpuPowerWatts."));
        s.gpuPowerWatts = v.toDouble();
    }
    if (map.contains(QStringLiteral("gpuVramUsedMegabytes"))) {
        const QVariant v = map.value(QStringLiteral("gpuVramUsedMegabytes"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: gpuVramUsedMegabytes."));
        s.gpuVramUsedMegabytes = v.toDouble();
    }
    if (map.contains(QStringLiteral("gpuVramTotalMegabytes"))) {
        const QVariant v = map.value(QStringLiteral("gpuVramTotalMegabytes"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: gpuVramTotalMegabytes."));
        s.gpuVramTotalMegabytes = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiCpuSome10"))) {
        const QVariant v = map.value(QStringLiteral("psiCpuSome10"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiCpuSome10."));
        s.psiCpuSome10 = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiCpuSome60"))) {
        const QVariant v = map.value(QStringLiteral("psiCpuSome60"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiCpuSome60."));
        s.psiCpuSome60 = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiMemSome10"))) {
        const QVariant v = map.value(QStringLiteral("psiMemSome10"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiMemSome10."));
        s.psiMemSome10 = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiMemSome60"))) {
        const QVariant v = map.value(QStringLiteral("psiMemSome60"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiMemSome60."));
        s.psiMemSome60 = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiMemFull10"))) {
        const QVariant v = map.value(QStringLiteral("psiMemFull10"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiMemFull10."));
        s.psiMemFull10 = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiMemFull60"))) {
        const QVariant v = map.value(QStringLiteral("psiMemFull60"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiMemFull60."));
        s.psiMemFull60 = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiIoSome10"))) {
        const QVariant v = map.value(QStringLiteral("psiIoSome10"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiIoSome10."));
        s.psiIoSome10 = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiIoSome60"))) {
        const QVariant v = map.value(QStringLiteral("psiIoSome60"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiIoSome60."));
        s.psiIoSome60 = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiIoFull10"))) {
        const QVariant v = map.value(QStringLiteral("psiIoFull10"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiIoFull10."));
        s.psiIoFull10 = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiIoFull60"))) {
        const QVariant v = map.value(QStringLiteral("psiIoFull60"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiIoFull60."));
        s.psiIoFull60 = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiCpuStallMsPerSec"))) {
        const QVariant v = map.value(QStringLiteral("psiCpuStallMsPerSec"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiCpuStallMsPerSec."));
        s.psiCpuStallMsPerSec = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiMemStallMsPerSec"))) {
        const QVariant v = map.value(QStringLiteral("psiMemStallMsPerSec"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiMemStallMsPerSec."));
        s.psiMemStallMsPerSec = v.toDouble();
    }
    if (map.contains(QStringLiteral("psiIoStallMsPerSec"))) {
        const QVariant v = map.value(QStringLiteral("psiIoStallMsPerSec"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: psiIoStallMsPerSec."));
        s.psiIoStallMsPerSec = v.toDouble();
    }
    if (map.contains(QStringLiteral("responsivenessScore"))) {
        const QVariant v = map.value(QStringLiteral("responsivenessScore"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: responsivenessScore."));
        s.responsivenessScore = v.toDouble();
    }
    if (map.contains(QStringLiteral("systemWatts"))) {
        const QVariant v = map.value(QStringLiteral("systemWatts"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: systemWatts."));
        s.systemWatts = v.toDouble();
    }
    if (map.contains(QStringLiteral("packageWatts"))) {
        const QVariant v = map.value(QStringLiteral("packageWatts"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: packageWatts."));
        s.packageWatts = v.toDouble();
    }
    if (map.contains(QStringLiteral("batteryWatts"))) {
        const QVariant v = map.value(QStringLiteral("batteryWatts"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: batteryWatts."));
        s.batteryWatts = v.toDouble();
    }
    if (map.contains(QStringLiteral("batteryPercent"))) {
        const QVariant v = map.value(QStringLiteral("batteryPercent"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: batteryPercent."));
        s.batteryPercent = v.toDouble();
    }
    if (map.contains(QStringLiteral("batteryHealthPercent"))) {
        const QVariant v = map.value(QStringLiteral("batteryHealthPercent"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: batteryHealthPercent."));
        s.batteryHealthPercent = v.toDouble();
    }
    if (map.contains(QStringLiteral("pl1LimitWatts"))) {
        const QVariant v = map.value(QStringLiteral("pl1LimitWatts"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: pl1LimitWatts."));
        s.pl1LimitWatts = v.toDouble();
    }
    if (map.contains(QStringLiteral("pl2LimitWatts"))) {
        const QVariant v = map.value(QStringLiteral("pl2LimitWatts"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: pl2LimitWatts."));
        s.pl2LimitWatts = v.toDouble();
    }
    if (map.contains(QStringLiteral("fanRpm"))) {
        const QVariant v = map.value(QStringLiteral("fanRpm"));
        if (!(integerValue(v, false) && v.toLongLong() <= std::numeric_limits<int>::max())) return fail(error, QStringLiteral("Invalid sample field: fanRpm."));
        s.fanRpm = v.toInt();
    }
    if (map.contains(QStringLiteral("ramUsedPercent"))) {
        const QVariant v = map.value(QStringLiteral("ramUsedPercent"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: ramUsedPercent."));
        s.ramUsedPercent = v.toDouble();
    }
    if (map.contains(QStringLiteral("ramUsedBytes"))) {
        const QVariant v = map.value(QStringLiteral("ramUsedBytes"));
        if (!(integerValue(v, true))) return fail(error, QStringLiteral("Invalid sample field: ramUsedBytes."));
        s.ramUsedBytes = v.toULongLong();
    }
    if (map.contains(QStringLiteral("ramTotalBytes"))) {
        const QVariant v = map.value(QStringLiteral("ramTotalBytes"));
        if (!(integerValue(v, true))) return fail(error, QStringLiteral("Invalid sample field: ramTotalBytes."));
        s.ramTotalBytes = v.toULongLong();
    }
    if (map.contains(QStringLiteral("swapUsedBytes"))) {
        const QVariant v = map.value(QStringLiteral("swapUsedBytes"));
        if (!(integerValue(v, true))) return fail(error, QStringLiteral("Invalid sample field: swapUsedBytes."));
        s.swapUsedBytes = v.toULongLong();
    }
    if (map.contains(QStringLiteral("swapTotalBytes"))) {
        const QVariant v = map.value(QStringLiteral("swapTotalBytes"));
        if (!(integerValue(v, true))) return fail(error, QStringLiteral("Invalid sample field: swapTotalBytes."));
        s.swapTotalBytes = v.toULongLong();
    }
    if (map.contains(QStringLiteral("zramCompressionRatio"))) {
        const QVariant v = map.value(QStringLiteral("zramCompressionRatio"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: zramCompressionRatio."));
        s.zramCompressionRatio = v.toDouble();
    }
    if (map.contains(QStringLiteral("zramOriginalBytes"))) {
        const QVariant v = map.value(QStringLiteral("zramOriginalBytes"));
        if (!(integerValue(v, true))) return fail(error, QStringLiteral("Invalid sample field: zramOriginalBytes."));
        s.zramOriginalBytes = v.toULongLong();
    }
    if (map.contains(QStringLiteral("zramCompressedBytes"))) {
        const QVariant v = map.value(QStringLiteral("zramCompressedBytes"));
        if (!(integerValue(v, true))) return fail(error, QStringLiteral("Invalid sample field: zramCompressedBytes."));
        s.zramCompressedBytes = v.toULongLong();
    }
    if (map.contains(QStringLiteral("zramMemoryUsedBytes"))) {
        const QVariant v = map.value(QStringLiteral("zramMemoryUsedBytes"));
        if (!(integerValue(v, true))) return fail(error, QStringLiteral("Invalid sample field: zramMemoryUsedBytes."));
        s.zramMemoryUsedBytes = v.toULongLong();
    }
    if (map.contains(QStringLiteral("diskReadMibPerSec"))) {
        const QVariant v = map.value(QStringLiteral("diskReadMibPerSec"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: diskReadMibPerSec."));
        s.diskReadMibPerSec = v.toDouble();
    }
    if (map.contains(QStringLiteral("diskWriteMibPerSec"))) {
        const QVariant v = map.value(QStringLiteral("diskWriteMibPerSec"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: diskWriteMibPerSec."));
        s.diskWriteMibPerSec = v.toDouble();
    }
    if (map.contains(QStringLiteral("netRxMibPerSec"))) {
        const QVariant v = map.value(QStringLiteral("netRxMibPerSec"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: netRxMibPerSec."));
        s.netRxMibPerSec = v.toDouble();
    }
    if (map.contains(QStringLiteral("netTxMibPerSec"))) {
        const QVariant v = map.value(QStringLiteral("netTxMibPerSec"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: netTxMibPerSec."));
        s.netTxMibPerSec = v.toDouble();
    }
    if (map.contains(QStringLiteral("throttleEventCount"))) {
        const QVariant v = map.value(QStringLiteral("throttleEventCount"));
        if (!(integerValue(v, false))) return fail(error, QStringLiteral("Invalid sample field: throttleEventCount."));
        s.throttleEventCount = v.toLongLong();
    }
    if (map.contains(QStringLiteral("newThrottleEvents"))) {
        const QVariant v = map.value(QStringLiteral("newThrottleEvents"));
        if (!(integerValue(v, false))) return fail(error, QStringLiteral("Invalid sample field: newThrottleEvents."));
        s.newThrottleEvents = v.toLongLong();
    }
    if (map.contains(QStringLiteral("batteryPresent"))) {
        const QVariant v = map.value(QStringLiteral("batteryPresent"));
        if (!(v.metaType().id() == QMetaType::Bool)) return fail(error, QStringLiteral("Invalid sample field: batteryPresent."));
        s.batteryPresent = v.toBool();
    }
    if (map.contains(QStringLiteral("onAcPower"))) {
        const QVariant v = map.value(QStringLiteral("onAcPower"));
        if (!(v.metaType().id() == QMetaType::Bool)) return fail(error, QStringLiteral("Invalid sample field: onAcPower."));
        s.onAcPower = v.toBool();
    }
    if (map.contains(QStringLiteral("activePowerProfile"))) {
        const QVariant v = map.value(QStringLiteral("activePowerProfile"));
        if (!(v.metaType().id() == QMetaType::QString)) return fail(error, QStringLiteral("Invalid sample field: activePowerProfile."));
        s.activePowerProfile = v.toString();
    }
    if (map.contains(QStringLiteral("throttled"))) {
        const QVariant v = map.value(QStringLiteral("throttled"));
        if (!(v.metaType().id() == QMetaType::Bool)) return fail(error, QStringLiteral("Invalid sample field: throttled."));
        s.throttled = v.toBool();
    }
    if (map.contains(QStringLiteral("severity"))) {
        const QVariant v = map.value(QStringLiteral("severity"));
        if (!(integerValue(v, false) && v.toInt() <= static_cast<int>(VerdictSeverity::Unknown))) return fail(error, QStringLiteral("Invalid sample field: severity."));
        s.severity = static_cast<VerdictSeverity>(v.toInt());
    }
    if (map.contains(QStringLiteral("verdictHeadline"))) {
        const QVariant v = map.value(QStringLiteral("verdictHeadline"));
        if (!(v.metaType().id() == QMetaType::QString)) return fail(error, QStringLiteral("Invalid sample field: verdictHeadline."));
        s.verdictHeadline = v.toString();
    }
    if (map.contains(QStringLiteral("verdictRuleId"))) {
        const QVariant v = map.value(QStringLiteral("verdictRuleId"));
        if (!(v.metaType().id() == QMetaType::QString)) return fail(error, QStringLiteral("Invalid sample field: verdictRuleId."));
        s.verdictRuleId = v.toString();
    }
    if (map.contains(QStringLiteral("verdictEvidence"))) {
        const QVariant v = map.value(QStringLiteral("verdictEvidence"));
        if (!(v.metaType().id() == QMetaType::QStringList || v.metaType().id() == QMetaType::QVariantList)) return fail(error, QStringLiteral("Invalid sample field: verdictEvidence."));
        for (const auto &item : v.toList()) if (item.metaType().id() != QMetaType::QString) return fail(error, QStringLiteral("Invalid diagnosis evidence."));
        s.verdictEvidence = v.toStringList();
    }
    if (map.contains(QStringLiteral("verdictDurationMs"))) {
        const QVariant v = map.value(QStringLiteral("verdictDurationMs"));
        if (!(integerValue(v, false))) return fail(error, QStringLiteral("Invalid sample field: verdictDurationMs."));
        s.verdictDurationMs = v.toLongLong();
    }
    if (map.contains(QStringLiteral("culpritName"))) {
        const QVariant v = map.value(QStringLiteral("culpritName"));
        if (!(v.metaType().id() == QMetaType::QString)) return fail(error, QStringLiteral("Invalid sample field: culpritName."));
        s.culpritName = v.toString();
    }
    if (map.contains(QStringLiteral("culpritPid"))) {
        const QVariant v = map.value(QStringLiteral("culpritPid"));
        if (!(integerValue(v, false) && v.toLongLong() <= std::numeric_limits<int>::max())) return fail(error, QStringLiteral("Invalid sample field: culpritPid."));
        s.culpritPid = v.toInt();
    }
    if (map.contains(QStringLiteral("culpritStartTimeTicks"))) {
        const QVariant v = map.value(QStringLiteral("culpritStartTimeTicks"));
        if (!(integerValue(v, true))) return fail(error, QStringLiteral("Invalid sample field: culpritStartTimeTicks."));
        s.culpritStartTimeTicks = v.toULongLong();
    }
    if (map.contains(QStringLiteral("processObservedAtMs"))) {
        const QVariant v = map.value(QStringLiteral("processObservedAtMs"));
        if (!(integerValue(v, false))) return fail(error, QStringLiteral("Invalid sample field: processObservedAtMs."));
        s.processObservedAtMs = v.toLongLong();
    }
    if (map.contains(QStringLiteral("culpritCpuPercent"))) {
        const QVariant v = map.value(QStringLiteral("culpritCpuPercent"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: culpritCpuPercent."));
        s.culpritCpuPercent = v.toDouble();
    }
    if (map.contains(QStringLiteral("culpritRssMb"))) {
        const QVariant v = map.value(QStringLiteral("culpritRssMb"));
        if (!((v.metaType().id() == QMetaType::Double || v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::LongLong) && std::isfinite(v.toDouble()))) return fail(error, QStringLiteral("Invalid sample field: culpritRssMb."));
        s.culpritRssMb = v.toDouble();
    }
    if (map.contains(QStringLiteral("metricQuality"))) {
        const QVariant v = map.value(QStringLiteral("metricQuality"));
        if (!(v.metaType().id() == QMetaType::QVariantMap)) return fail(error, QStringLiteral("Invalid sample field: metricQuality."));
        s.metricQuality = v.toMap();
    }
    if (map.contains(QStringLiteral("gpuDevices"))) {
        const QVariant v = map.value(QStringLiteral("gpuDevices"));
        if (!(v.metaType().id() == QMetaType::QVariantList)) return fail(error, QStringLiteral("Invalid sample field: gpuDevices."));
        s.gpuDevices = v.toList();
    }
    if (map.contains(QStringLiteral("storageDevices"))) {
        const QVariant v = map.value(QStringLiteral("storageDevices"));
        if (!(v.metaType().id() == QMetaType::QVariantList)) return fail(error, QStringLiteral("Invalid sample field: storageDevices."));
        s.storageDevices = v.toList();
    }
    if (map.contains(QStringLiteral("networkDevices"))) {
        const QVariant v = map.value(QStringLiteral("networkDevices"));
        if (!(v.metaType().id() == QMetaType::QVariantList)) return fail(error, QStringLiteral("Invalid sample field: networkDevices."));
        s.networkDevices = v.toList();
    }
    if (map.contains(QStringLiteral("batteries"))) {
        const QVariant v = map.value(QStringLiteral("batteries"));
        if (!(v.metaType().id() == QMetaType::QVariantList)) return fail(error, QStringLiteral("Invalid sample field: batteries."));
        s.batteries = v.toList();
    }
    if (map.contains(QStringLiteral("processObservations"))) {
        const QVariant v = map.value(QStringLiteral("processObservations"));
        if (!(v.metaType().id() == QMetaType::QVariantList)) return fail(error, QStringLiteral("Invalid sample field: processObservations."));
        s.processObservations = v.toList();
    }
    if (map.contains(QStringLiteral("captureId"))) {
        const QVariant v = map.value(QStringLiteral("captureId"));
        if (!(v.metaType().id() == QMetaType::QString)) return fail(error, QStringLiteral("Invalid sample field: captureId."));
        s.captureId = v.toString();
    }
    if (map.contains(QStringLiteral("engineVersion"))) {
        const QVariant v = map.value(QStringLiteral("engineVersion"));
        if (!(v.metaType().id() == QMetaType::QString)) return fail(error, QStringLiteral("Invalid sample field: engineVersion."));
        s.engineVersion = v.toString();
    }
    if (map.contains(QStringLiteral("verdictRecommendation"))) {
        const QVariant v = map.value(QStringLiteral("verdictRecommendation"));
        if (!(v.metaType().id() == QMetaType::QString)) return fail(error, QStringLiteral("Invalid sample field: verdictRecommendation."));
        s.verdictRecommendation = v.toString();
    }
    if (map.contains(QStringLiteral("verdictCausalChain"))) {
        const QVariant v = map.value(QStringLiteral("verdictCausalChain"));
        if (!(v.metaType().id() == QMetaType::QString)) return fail(error, QStringLiteral("Invalid sample field: verdictCausalChain."));
        s.verdictCausalChain = v.toString();
    }
    if (s.sampleId == 0) return fail(error, QStringLiteral("Sample identities must be positive."));
    *sample = std::move(s);
    return true;
}
QByteArray SessionStore::encode(const SessionSnapshot &snapshot, bool includeProcessDetails, QString *error) {
    if (snapshot.samples.size() > MaximumSamples) { fail(error, QStringLiteral("Session exceeds 7,201 telemetry samples.")); return {}; }
    QVariantList samples;
    for (const auto &s : snapshot.samples) samples.append(sampleToMap(s, true));
    QVariantMap root{{QStringLiteral("format"), QStringLiteral("pulse-session")},
        {QStringLiteral("version"), 1}, {QStringLiteral("recordingId"), snapshot.recordingId},
        {QStringLiteral("engineVersion"), snapshot.engineVersion}, {QStringLiteral("samples"), samples},
        {QStringLiteral("incidents"), snapshot.incidents}, {QStringLiteral("markers"), snapshot.markers},
        {QStringLiteral("metadata"), snapshot.metadata}, {QStringLiteral("processDetailsIncluded"), includeProcessDetails}};
    if (!includeProcessDetails) root = redactProcessDetails(root).toMap();
    const auto bytes = QJsonDocument(QJsonObject::fromVariantMap(root)).toJson(QJsonDocument::Compact);
    SessionSnapshot verified;
    if (!decode(bytes, &verified, error)) return {};
    return bytes;
}
bool SessionStore::decode(const QByteArray &data, SessionSnapshot *snapshot, QString *error) {
    if (!snapshot) return fail(error, QStringLiteral("No session destination was provided."));
    if (data.size() > MaximumBytes) return fail(error, QStringLiteral("Session exceeds the 64 MiB size limit."));
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return fail(error, QStringLiteral("The session is not a valid JSON object."));
    const auto root = doc.object();
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("pulse-session")
        || root.value(QStringLiteral("version")).toDouble() != 1.0)
        return fail(error, QStringLiteral("Unsupported session format or version."));
    if (!root.value(QStringLiteral("samples")).isArray() || !root.value(QStringLiteral("incidents")).isArray()
        || !root.value(QStringLiteral("markers")).isArray() || !root.value(QStringLiteral("metadata")).isObject()
        || !root.value(QStringLiteral("recordingId")).isString() || !root.value(QStringLiteral("engineVersion")).isString())
        return fail(error, QStringLiteral("The session structure is incomplete."));
    const auto array = root.value(QStringLiteral("samples")).toArray();
    if (array.isEmpty()) return fail(error, QStringLiteral("The session contains no recorded samples."));
    if (array.size() > MaximumSamples) return fail(error, QStringLiteral("Session exceeds 7,201 telemetry samples."));
    SessionSnapshot candidate;
    candidate.recordingId = root.value(QStringLiteral("recordingId")).toString();
    candidate.engineVersion = root.value(QStringLiteral("engineVersion")).toString();
    candidate.metadata = root.value(QStringLiteral("metadata")).toObject().toVariantMap();
    QSet<quint64> ids;
    qint64 lastTime = -1;
    quint64 previousId = 0;
    const auto required = redactProcessDetails(sampleToMap(FlightSample{}, false)).toMap();
    for (const auto &value : array) {
        FlightSample s;
        if (!value.isObject()) return fail(error, QStringLiteral("A sample is not an object."));
        const auto sampleMap = value.toObject().toVariantMap();
        for (auto it = required.cbegin(); it != required.cend(); ++it)
            if (!sampleMap.contains(it.key())) return fail(error, QStringLiteral("Incomplete telemetry sample: %1.").arg(it.key()));
        if (!sampleFromMap(sampleMap, &s, error)) return false;
        const qint64 time = s.monotonicMs > 0 ? s.monotonicMs : s.timestampMs;
        if (ids.contains(s.sampleId) || s.sampleId <= previousId || time < lastTime)
            return fail(error, QStringLiteral("Sample identities or timestamps are out of order."));
        ids.insert(s.sampleId); lastTime = time; previousId = s.sampleId;
        candidate.samples.append(std::move(s));
    }
    QSet<QString> recordIds;
    const auto loadRecords = [&](const QJsonArray &records, QVariantList &destination, const QString &identityKey) {
        for (const auto &record : records) {
            if (!record.isObject()) return fail(error, QStringLiteral("An incident or marker is malformed."));
            const auto map = record.toObject().toVariantMap();
            if (map.value(identityKey).metaType().id() != QMetaType::QString)
                return fail(error, QStringLiteral("Incident and marker identities must be strings."));
            const QString id = map.value(identityKey).toString();
            if (id.isEmpty() || recordIds.contains(id)) return fail(error, QStringLiteral("Incident and marker identities must be unique."));
            recordIds.insert(id);
            for (const auto &key : {QStringLiteral("sampleId"), QStringLiteral("startSampleId"), QStringLiteral("endSampleId"), QStringLiteral("recoverySampleId")}) {
                if (!map.contains(key)) continue;
                const auto v = map.value(key);
                if (!integerValue(v, true) || !ids.contains(v.toULongLong()))
                    return fail(error, QStringLiteral("An incident or marker references a missing sample."));
            }
            for (const auto &key : {QStringLiteral("startMs"), QStringLiteral("endMs"), QStringLiteral("timestampMs"), QStringLiteral("durationSeconds")}) {
                if (map.contains(key) && !integerValue(map.value(key), false))
                    return fail(error, QStringLiteral("An incident or marker has an invalid timestamp."));
            }
            if (map.contains(QStringLiteral("startMs")) && map.contains(QStringLiteral("endMs"))
                && map.value(QStringLiteral("startMs")).toLongLong() > map.value(QStringLiteral("endMs")).toLongLong())
                return fail(error, QStringLiteral("An incident time range is reversed."));
            if (map.contains(QStringLiteral("startSampleId")) && map.contains(QStringLiteral("endSampleId"))
                && map.value(QStringLiteral("startSampleId")).toULongLong() > map.value(QStringLiteral("endSampleId")).toULongLong())
                return fail(error, QStringLiteral("An incident sample range is reversed."));
            for (const auto &key : {QStringLiteral("active"), QStringLiteral("partial"), QStringLiteral("gapTerminated")})
                if (map.contains(key) && map.value(key).metaType().id() != QMetaType::Bool)
                    return fail(error, QStringLiteral("An incident state is invalid."));
            if (map.contains(QStringLiteral("label")) && map.value(QStringLiteral("label")).metaType().id() != QMetaType::QString)
                return fail(error, QStringLiteral("A marker label is invalid."));
            destination.append(map);
        }
        return true;
    };
    if (!loadRecords(root.value(QStringLiteral("incidents")).toArray(), candidate.incidents, QStringLiteral("incidentId"))
        || !loadRecords(root.value(QStringLiteral("markers")).toArray(), candidate.markers, QStringLiteral("markerId"))) return false;
    for (const auto &key : {QStringLiteral("selectionStartSampleId"), QStringLiteral("selectionEndSampleId")}) {
        if (!candidate.metadata.contains(key)) continue;
        const auto value = candidate.metadata.value(key);
        if (!integerValue(value, true) || !ids.contains(value.toULongLong()))
            return fail(error, QStringLiteral("The selected range references a missing sample."));
    }
    if (candidate.metadata.contains(QStringLiteral("selectionStartSampleId"))
        && candidate.metadata.contains(QStringLiteral("selectionEndSampleId"))
        && candidate.metadata.value(QStringLiteral("selectionStartSampleId")).toULongLong()
            > candidate.metadata.value(QStringLiteral("selectionEndSampleId")).toULongLong())
        return fail(error, QStringLiteral("The selected range is reversed."));
    *snapshot = std::move(candidate);
    return true;
}
bool SessionStore::write(const QString &path, const SessionSnapshot &snapshot, bool includeProcessDetails, QString *error) {
    const auto bytes = encode(snapshot, includeProcessDetails, error);
    if (bytes.isEmpty()) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, QStringLiteral("Could not open the session file for writing. Check the destination and write permission. %1").arg(file.errorString()));
    if (file.write(bytes) != bytes.size() || !file.commit())
        return fail(error, QStringLiteral("The session was not saved. Check free disk space and write permission. %1").arg(file.errorString()));
    return true;
}
bool SessionStore::read(const QString &path, SessionSnapshot *snapshot, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    if (file.size() > MaximumBytes) return fail(error, QStringLiteral("Session exceeds the 64 MiB size limit."));
    const auto bytes = file.read(MaximumBytes + 1);
    if (file.error() != QFileDevice::NoError) return fail(error, file.errorString());
    return decode(bytes, snapshot, error);
}
} // namespace LoofiPulse
