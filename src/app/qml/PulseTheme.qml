pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n

QtObject {
    id: theme

    property color bgColor: "#20232a"
    property color disabledColor: "#7f8c8d"
    property color negativeColor: "#da4453"
    property color neutralColor: "#f67400"
    property color positiveColor: "#27ae60"

    // Detect dark vs light KDE color scheme dynamically from bgColor
    readonly property bool isDark: {
        var c = bgColor
        var luma = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b
        return luma < 0.5
    }

    // Surface & border tokens
    readonly property color windowBg: bgColor
    readonly property color surfaceColor: isDark
        ? Qt.tint(bgColor, Qt.rgba(1, 1, 1, 0.045))
        : Qt.tint(bgColor, Qt.rgba(0, 0, 0, 0.025))
    readonly property color elevatedSurfaceColor: isDark
        ? Qt.tint(bgColor, Qt.rgba(1, 1, 1, 0.075))
        : Qt.tint(bgColor, Qt.rgba(0, 0, 0, 0.045))
    readonly property color subtleSurfaceColor: isDark
        ? Qt.rgba(1, 1, 1, 0.03)
        : Qt.rgba(0, 0, 0, 0.025)
    readonly property color cardNestedBg: isDark
        ? Qt.rgba(0, 0, 0, 0.22)
        : Qt.rgba(0, 0, 0, 0.035)
    readonly property color trackColor: isDark
        ? Qt.rgba(1, 1, 1, 0.08)
        : Qt.rgba(0, 0, 0, 0.07)
    readonly property color cardBorderColor: isDark
        ? Qt.rgba(1, 1, 1, 0.08)
        : Qt.rgba(0, 0, 0, 0.09)
    readonly property color cardHoverBorderColor: isDark
        ? Qt.rgba(1, 1, 1, 0.22)
        : Qt.rgba(0, 0, 0, 0.20)
    readonly property color subtleBorderColor: isDark
        ? Qt.rgba(1, 1, 1, 0.05)
        : Qt.rgba(0, 0, 0, 0.05)
    readonly property color tableHeaderBg: isDark
        ? Qt.rgba(1, 1, 1, 0.04)
        : Qt.rgba(0, 0, 0, 0.035)
    readonly property color tableRowHoverBg: isDark
        ? Qt.rgba(1, 1, 1, 0.06)
        : Qt.rgba(0, 0, 0, 0.04)
    readonly property color tableRowAltBg: isDark
        ? Qt.rgba(1, 1, 1, 0.015)
        : Qt.rgba(0, 0, 0, 0.015)

    // KDE 6 Glass & Accent Glow tokens
    readonly property color glassCardBg: isDark
        ? Qt.rgba(bgColor.r, bgColor.g, bgColor.b, 0.78)
        : Qt.rgba(bgColor.r, bgColor.g, bgColor.b, 0.88)
    readonly property color glassElevatedBg: isDark
        ? Qt.tint(bgColor, Qt.rgba(1, 1, 1, 0.08))
        : Qt.tint(bgColor, Qt.rgba(0, 0, 0, 0.04))
    readonly property color glassBorder: isDark
        ? Qt.rgba(1, 1, 1, 0.12)
        : Qt.rgba(0, 0, 0, 0.10)
    readonly property color glassGlowCritical: Qt.rgba(negativeColor.r, negativeColor.g, negativeColor.b, 0.45)
    readonly property color glassGlowWarning: Qt.rgba(neutralColor.r, neutralColor.g, neutralColor.b, 0.40)
    readonly property color glassGlowNominal: Qt.rgba(positiveColor.r, positiveColor.g, positiveColor.b, 0.25)

    // Semantic telemetry channel colors (high contrast in both dark and light mode)
    readonly property color tempAccent: isDark ? "#fb923c" : "#d97706"
    readonly property color cpuAccent: isDark ? "#38bdf8" : "#0284c7"
    readonly property color psiAccent: isDark ? "#34d399" : "#059669"
    readonly property color powerAccent: isDark ? "#a78bfa" : "#7c3aed"
    readonly property color batteryAccent: isDark ? "#facc15" : "#a16207"
    readonly property color gpuAccent: isDark ? "#f472b6" : "#db2777"
    readonly property color storageAccent: isDark ? "#2dd4bf" : "#0d9488"

    // Standard radius tokens
    readonly property int cardRadius: 10
    readonly property int innerRadius: 7
    readonly property int pillRadius: 6

    function severityColor(sev) {
        if (sev === 4) return disabledColor
        if (sev === 3) return negativeColor
        if (sev === 2) return negativeColor
        if (sev === 1) return neutralColor
        return positiveColor
    }

    function severityBgColor(sev) {
        var base = severityColor(sev)
        return Qt.rgba(base.r, base.g, base.b, isDark ? 0.12 : 0.08)
    }

    function severityBorderColor(sev) {
        var base = severityColor(sev)
        return Qt.rgba(base.r, base.g, base.b, isDark ? 0.45 : 0.35)
    }

    function severityGlowColor(sev) {
        if (sev === 3 || sev === 2) return glassGlowCritical
        if (sev === 1) return glassGlowWarning
        return glassGlowNominal
    }

    function severityBadgeText(sev) {
        if (sev === 4) return KI18n.i18n("WARMING UP")
        if (sev === 3) return KI18n.i18n("THROTTLING")
        if (sev === 2) return KI18n.i18n("BOTTLENECK")
        if (sev === 1) return KI18n.i18n("ELEVATED")
        return KI18n.i18n("NOMINAL")
    }

    function severityIcon(sev) {
        if (sev === 4) return "state-sync"
        if (sev === 3) return "dialog-error"
        if (sev === 2) return "dialog-warning"
        if (sev === 1) return "dialog-information"
        return "dialog-ok"
    }

    function accentTint(accentColor, alphaValue) {
        return Qt.rgba(accentColor.r, accentColor.g, accentColor.b, alphaValue)
    }

    function channelColor(key) {
        if (key === "temp") return tempAccent
        if (key === "cpu") return cpuAccent
        if (key === "psi") return psiAccent
        if (key === "power") return powerAccent
        if (key === "battery") return batteryAccent
        if (key === "gpu") return gpuAccent
        if (key === "storage") return storageAccent
        return cpuAccent
    }
}
