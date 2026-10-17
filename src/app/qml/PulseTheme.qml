pragma ComponentBehavior: Bound
import QtQuick

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
        ? Qt.tint(bgColor, Qt.rgba(1, 1, 1, 0.08))
        : Qt.tint(bgColor, Qt.rgba(0, 0, 0, 0.05))
    readonly property color subtleSurfaceColor: isDark
        ? Qt.rgba(1, 1, 1, 0.035)
        : Qt.rgba(0, 0, 0, 0.035)
    readonly property color trackColor: isDark
        ? Qt.rgba(1, 1, 1, 0.09)
        : Qt.rgba(0, 0, 0, 0.08)
    readonly property color cardBorderColor: isDark
        ? Qt.rgba(1, 1, 1, 0.11)
        : Qt.rgba(0, 0, 0, 0.12)
    readonly property color subtleBorderColor: isDark
        ? Qt.rgba(1, 1, 1, 0.07)
        : Qt.rgba(0, 0, 0, 0.08)

    // Semantic telemetry channel colors (high contrast in both dark and light mode)
    readonly property color tempAccent: isDark ? "#fb923c" : "#d97706"
    readonly property color cpuAccent: isDark ? "#38bdf8" : "#0284c7"
    readonly property color psiAccent: isDark ? "#34d399" : "#059669"
    readonly property color powerAccent: isDark ? "#a78bfa" : "#7c3aed"
    readonly property color batteryAccent: isDark ? "#facc15" : "#a16207"
    readonly property color gpuAccent: isDark ? "#f472b6" : "#db2777"
    readonly property color storageAccent: isDark ? "#2dd4bf" : "#0d9488"

    // Standard radius tokens
    readonly property int cardRadius: 12
    readonly property int innerRadius: 8
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
        return Qt.rgba(base.r, base.g, base.b, isDark ? 0.13 : 0.09)
    }

    function severityBorderColor(sev) {
        var base = severityColor(sev)
        return Qt.rgba(base.r, base.g, base.b, isDark ? 0.55 : 0.45)
    }

    function severityBadgeText(sev) {
        if (sev === 4) return "WARMING UP"
        if (sev === 3) return "THROTTLING"
        if (sev === 2) return "BOTTLENECK"
        if (sev === 1) return "ELEVATED"
        return "NOMINAL"
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
