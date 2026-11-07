// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.ki18n
import "TemperatureLabels.js" as TemperatureLabels
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

Item {
    id: fullRoot

    required property var controller

    Layout.minimumWidth: Kirigami.Units.gridUnit * 18
    Layout.preferredWidth: Kirigami.Units.gridUnit * 23
    Layout.minimumHeight: Kirigami.Units.gridUnit * 18
    Layout.preferredHeight: Kirigami.Units.gridUnit * 25

    readonly property bool isDark: {
        var c = Kirigami.Theme.backgroundColor
        return (0.299 * c.r + 0.587 * c.g + 0.114 * c.b) < 0.5
    }

    readonly property PulseTheme theme: PulseTheme {
        bgColor: Kirigami.Theme.backgroundColor
        textColor: Kirigami.Theme.textColor
        highlightColor: Kirigami.Theme.highlightColor
        disabledColor: Kirigami.Theme.disabledTextColor
        negativeColor: Kirigami.Theme.negativeTextColor
        neutralColor: Kirigami.Theme.neutralTextColor
        positiveColor: Kirigami.Theme.positiveTextColor
    }

    readonly property color cardSurface: isDark
        ? Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(1, 1, 1, 0.045))
        : Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(0, 0, 0, 0.025))
    readonly property color cardBorder: isDark
        ? Qt.rgba(1, 1, 1, 0.08)
        : Qt.rgba(0, 0, 0, 0.08)
    readonly property color trackBg: isDark
        ? Qt.rgba(1, 1, 1, 0.08)
        : Qt.rgba(0, 0, 0, 0.07)

    readonly property color tempAccent: isDark ? "#fb923c" : "#d97706"
    readonly property color cpuAccent: isDark ? "#38bdf8" : "#0284c7"
    readonly property color psiAccent: isDark ? "#34d399" : "#059669"
    readonly property color powerAccent: isDark ? "#a78bfa" : "#7c3aed"
    readonly property color gpuAccent: isDark ? "#f472b6" : "#db2777"

    property string sparkChannel: "temp"

    function channelColor(key) {
        if (key === "temp") return tempAccent
        if (key === "cpu") return cpuAccent
        if (key === "psi") return psiAccent
        if (key === "power") return powerAccent
        if (key === "battery") return cpuAccent
        if (key === "gpu") return gpuAccent
        return cpuAccent
    }

    function sparkChannelDescription() {
        const mins = Math.round((fullRoot.controller.historyRetentionSeconds || 300) / 60)
        if (sparkChannel === "cpu") return KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU load · last ") + mins + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "m · 0–100%")
        if (sparkChannel === "psi") return KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Responsiveness index · last ") + mins + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "m · 0–100 (higher is better)")
        if (sparkChannel === "power") return KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU package power · last ") + mins + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "m · W")
        if (sparkChannel === "battery") return KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Battery discharge · last ") + mins + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "m · W")
        return KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU temperature · last ") + mins + "m · " + TemperatureLabels.formatTemperature(30, fullRoot.controller.temperatureUnit) + "–" + TemperatureLabels.formatTemperature(100, fullRoot.controller.temperatureUnit)
    }

    function sparkChannelNowText() {
        if (sparkChannel === "cpu") {
            return fullRoot.controller.cpuLoadAvailable
                   ? Math.round(fullRoot.controller.cpuLoadPercent) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "% now") : ""
        }
        if (sparkChannel === "psi") {
            return fullRoot.controller.psiAvailable
                   ? fullRoot.controller.responsivenessScore.toFixed(1) + "/100 now" : ""
        }
        if (sparkChannel === "battery") {
            return fullRoot.controller.batteryPowerAvailable && fullRoot.controller.batteryStatus === "Discharging"
                   ? fullRoot.controller.batteryWatts.toFixed(1) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " W now") : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Unavailable or not discharging")
        }
        if (sparkChannel === "power") {
            return fullRoot.controller.packagePowerAvailable
                   ? fullRoot.controller.packageWatts.toFixed(1) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "W now") : ""
        }
        return fullRoot.controller.cpuTempAvailable
               ? TemperatureLabels.formatTemperature(fullRoot.controller.cpuPackageTemp, fullRoot.controller.temperatureUnit) + " " + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "now") : ""
    }

    function severityColor(sev) {
        if (sev === 4) return Kirigami.Theme.disabledTextColor
        if (sev === 3 || sev === 2) return Kirigami.Theme.negativeTextColor
        if (sev === 1) return Kirigami.Theme.neutralTextColor
        return Kirigami.Theme.positiveTextColor
    }

    function severityBgColor(sev) {
        var c = severityColor(sev)
        return Qt.rgba(c.r, c.g, c.b, isDark ? 0.12 : 0.08)
    }

    function severityBadgeText(sev) {
        if (sev === 4) return KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "INITIALIZING")
        if (sev === 3) return KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "THROTTLING")
        if (sev === 2) return KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "BOTTLENECK")
        if (sev === 1) return KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "ELEVATED")
        return KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "NOMINAL")
    }

    function qualityText(key, label) {
        const metadata = controller.metricMetadata || {}
        const item = metadata[key]
        if (!item) return label + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", ": Unavailable — awaiting a sample.")
        const state = item.state === "available" ? KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Measured")
                    : item.state === "calculated" ? KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Calculated")
                    : item.state === "estimated" ? KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Estimated")
                    : item.state === "warming-up" ? KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Initializing…")
                    : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Unavailable")
        const explanation = item.detail || (item.state === "unavailable" ? KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "No valid reading from this source.") : "")
        const sampledAt = item.sampledAtMs > 0
                        ? Qt.formatTime(new Date(item.sampledAtMs), "HH:mm:ss")
                        : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Awaiting first sample")
        return label + ": " + state + (explanation ? " — " + explanation : "")
               + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " · Source: ") + (item.source || "Unknown") + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " · Sample: ") + sampledAt
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + Kirigami.Units.mediumSpacing * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentColumn
            width: parent.width - Kirigami.Units.mediumSpacing * 2
            x: Kirigami.Units.mediumSpacing
            y: Kirigami.Units.mediumSpacing
            spacing: Kirigami.Units.mediumSpacing

            // ==========================================
            // 1. COMPACT HEADER & PRIMARY ACTIONS
            // ==========================================
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "utilities-system-monitor"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                }

                Kirigami.Heading {
                    level: 4
                    text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Loofi Pulse")
                    Layout.fillWidth: true
                }

                // Status Badge
                Rectangle {
                    radius: fullRoot.theme.pillRadius
                    color: fullRoot.severityBgColor(fullRoot.controller.verdictSeverity)
                    border.width: 1
                    border.color: fullRoot.severityColor(fullRoot.controller.verdictSeverity)
                    implicitWidth: statusBadgeLbl.implicitWidth + 10
                    implicitHeight: statusBadgeLbl.implicitHeight + 4

                    PlasmaComponents3.Label {
                        id: statusBadgeLbl
                        anchors.centerIn: parent
                        text: fullRoot.severityBadgeText(fullRoot.controller.verdictSeverity)
                        color: fullRoot.severityColor(fullRoot.controller.verdictSeverity)
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    }
                }

                // Primary Launch Button
                PlasmaComponents3.Button {
                    icon.name: "window-new"
                    text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Open App")
                    Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Open Loofi Pulse")
                    onClicked: fullRoot.controller.launchStudioApp("overview")
                }
            }

            // Consolidated Secondary Actions Toolbar
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents3.Button {
                    Layout.fillWidth: true
                    icon.name: "bookmark-new"
                    text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Mark Incident")
                    Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Mark incident now and launch timeline")
                    onClicked: fullRoot.controller.markProblemAndLaunch()
                }

                PlasmaComponents3.Button {
                    icon.name: "notifications-disabled"
                    text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Snooze 15m")
                    Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Snooze notifications for 15 minutes")
                    onClicked: fullRoot.controller.snoozeNotifications()
                }

                PlasmaComponents3.Button {
                    icon.name: "edit-copy"
                    text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Copy")
                    Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Copy current incident report")
                    onClicked: fullRoot.controller.copyIncidentReportToClipboard()
                }

                PlasmaComponents3.Button {
                    icon.name: "utilities-system-monitor"
                    Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Open KDE System Monitor process view")
                    onClicked: fullRoot.controller.openSystemMonitor()
                }
            }

            // Action status notification banner (if active)
            RowLayout {
                Layout.fillWidth: true
                visible: fullRoot.controller.lastActionStatus.length > 0
                spacing: 6

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.controller.lastActionStatus
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    color: fullRoot.controller.lastActionSucceeded
                           ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor
                }
                PlasmaComponents3.Button {
                    visible: fullRoot.controller.lastActionPath.length > 0
                    text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Open folder")
                    icon.name: "folder-open"
                    onClicked: fullRoot.controller.openReportsFolder()
                }
            }

            // ==========================================
            // 2. ACTIVE PROBLEM BANNER (CONDITIONAL)
            // ==========================================
            PulseCard {
                Layout.fillWidth: true
                visible: fullRoot.controller.verdictSeverity > 0
                implicitHeight: alertCol.implicitHeight + padding * 2
                padding: Kirigami.Units.smallSpacing * 2
                accentColor: fullRoot.severityColor(fullRoot.controller.verdictSeverity)
                accentStrength: 0.15

                ColumnLayout {
                    id: alertCol
                    width: parent.width
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Kirigami.Icon {
                            source: "dialog-warning"
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            color: fullRoot.severityColor(fullRoot.controller.verdictSeverity)
                        }

                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", fullRoot.controller.verdictHeadline)
                            font.bold: true
                            color: fullRoot.severityColor(fullRoot.controller.verdictSeverity)
                            wrapMode: Text.WordWrap
                        }
                    }

                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        visible: fullRoot.controller.culpritName.length > 0
                        text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Culprit: ") + fullRoot.controller.culpritName
                              + (fullRoot.controller.culpritPid > 0 ? " (" + fullRoot.controller.culpritPid + ")" : "")
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.9
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Item { Layout.fillWidth: true }
                        PlasmaComponents3.Button {
                            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Investigate in Pulse")
                            icon.name: "tools-report-bug"
                            onClicked: fullRoot.controller.launchStudioApp("pressure", "overview")
                        }
                    }
                }
            }

            // ==========================================
            // 3. KEY HARDWARE METRIC CARDS (2x2 GRID)
            // ==========================================
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: Kirigami.Units.smallSpacing
                columnSpacing: Kirigami.Units.smallSpacing

                // 1. CPU & SoC
                PlasmaComponents3.Button {
                    id: cpuCard
                    Layout.fillWidth: true
                    implicitHeight: 64
                    Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Open CPU and Power details")
                    onClicked: fullRoot.controller.launchStudioApp("power", "cpu")

                    contentItem: ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Rectangle { width: 6; height: 6; radius: 3; color: fullRoot.cpuAccent }
                            PlasmaComponents3.Label {
                                text: "CPU"
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                opacity: 0.8
                                Layout.fillWidth: true
                            }
                            PlasmaComponents3.Label {
                                text: fullRoot.controller.cpuTempAvailable
                                      ? TemperatureLabels.formatTemperature(fullRoot.controller.cpuPackageTemp, fullRoot.controller.temperatureUnit) : "—"
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                color: fullRoot.tempAccent
                            }
                        }

                        PlasmaComponents3.Label {
                            text: fullRoot.controller.cpuLoadAvailable ? Math.round(fullRoot.controller.cpuLoadPercent) + "%" : "—"
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 1
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 3
                            radius: 1.5
                            color: fullRoot.trackBg
                            Rectangle {
                                width: parent.width * Math.max(0.02, Math.min(1.0, (fullRoot.controller.cpuLoadPercent || 0) / 100))
                                height: parent.height
                                radius: 1.5
                                color: fullRoot.cpuAccent
                            }
                        }
                    }
                }

                // 2. Memory & PSI
                PlasmaComponents3.Button {
                    id: memCard
                    Layout.fillWidth: true
                    implicitHeight: 64
                    Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Open Memory and Pressure details")
                    onClicked: fullRoot.controller.launchStudioApp("pressure", "overview")

                    contentItem: ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Rectangle { width: 6; height: 6; radius: 3; color: fullRoot.psiAccent }
                            PlasmaComponents3.Label {
                                text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "RAM & PSI")
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                opacity: 0.8
                                Layout.fillWidth: true
                            }
                            PlasmaComponents3.Label {
                                text: fullRoot.controller.psiAvailable ? fullRoot.controller.responsivenessScore.toFixed(0) + "/100" : "—"
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                color: fullRoot.psiAccent
                            }
                        }

                        PlasmaComponents3.Label {
                            text: fullRoot.controller.memoryAvailable ? Math.round(fullRoot.controller.ramUsedPercent) + "%" : "—"
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 1
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 3
                            radius: 1.5
                            color: fullRoot.trackBg
                            Rectangle {
                                width: parent.width * Math.max(0.02, Math.min(1.0, (fullRoot.controller.ramUsedPercent || 0) / 100))
                                height: parent.height
                                radius: 1.5
                                color: fullRoot.psiAccent
                            }
                        }
                    }
                }

                // 3. GPU & Storage
                PlasmaComponents3.Button {
                    id: gpuCard
                    Layout.fillWidth: true
                    implicitHeight: 64
                    Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Open GPU and sensor details")
                    onClicked: fullRoot.controller.launchStudioApp("power", "gpu")

                    contentItem: ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Rectangle { width: 6; height: 6; radius: 3; color: fullRoot.gpuAccent }
                            PlasmaComponents3.Label {
                                text: "GPU"
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                opacity: 0.8
                                Layout.fillWidth: true
                            }
                            PlasmaComponents3.Label {
                                text: fullRoot.controller.nvmeTempAvailable
                                      ? "NVMe " + TemperatureLabels.formatTemperature(fullRoot.controller.nvmeTempCelsius, fullRoot.controller.temperatureUnit) : ""
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize * 0.9
                                opacity: 0.8
                            }
                        }

                        PlasmaComponents3.Label {
                            text: fullRoot.controller.gpuBusyAvailable
                                  ? Math.round(fullRoot.controller.gpuBusyPercent) + "%"
                                  : (fullRoot.controller.gpuTempAvailable ? TemperatureLabels.formatTemperature(fullRoot.controller.gpuTempCelsius, fullRoot.controller.temperatureUnit) : "—")
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 1
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 3
                            radius: 1.5
                            color: fullRoot.trackBg
                            Rectangle {
                                width: parent.width * Math.max(0.02, Math.min(1.0, (fullRoot.controller.gpuBusyPercent || 0) / 100))
                                height: parent.height
                                radius: 1.5
                                color: fullRoot.gpuAccent
                            }
                        }
                    }
                }

                // 4. Power & Battery
                PlasmaComponents3.Button {
                    id: pwrCard
                    Layout.fillWidth: true
                    implicitHeight: 64
                    Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Open Power and Battery details")
                    onClicked: fullRoot.controller.launchStudioApp("power", "battery")

                    contentItem: ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Rectangle { width: 6; height: 6; radius: 3; color: fullRoot.powerAccent }
                            PlasmaComponents3.Label {
                                text: fullRoot.controller.batteryPresent
                                      ? KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Battery")
                                      : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Power")
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                opacity: 0.8
                                Layout.fillWidth: true
                            }
                            PlasmaComponents3.Label {
                                text: fullRoot.controller.batteryPresent
                                      ? Math.round(fullRoot.controller.batteryPercent) + "%"
                                      : (fullRoot.controller.onAcPower ? "AC" : "")
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                color: fullRoot.powerAccent
                            }
                        }

                        PlasmaComponents3.Label {
                            text: fullRoot.controller.packagePowerAvailable
                                  ? fullRoot.controller.packageWatts.toFixed(1) + " W" : "—"
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 1
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 3
                            radius: 1.5
                            color: fullRoot.trackBg
                            Rectangle {
                                width: parent.width * Math.max(0.02, Math.min(1.0, (fullRoot.controller.packageWatts || 0) / 65))
                                height: parent.height
                                radius: 1.5
                                color: fullRoot.powerAccent
                            }
                        }
                    }
                }
            }

            // ==========================================
            // 4. MINI LIVE SPARKLINE
            // ==========================================
            PulseCard {
                Layout.fillWidth: true
                implicitHeight: sparkCol.implicitHeight + padding * 2
                padding: Kirigami.Units.smallSpacing * 1.5

                ColumnLayout {
                    id: sparkCol
                    width: parent.width
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        PlasmaComponents3.Label {
                            text: fullRoot.sparkChannelDescription()
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            opacity: 0.8
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        PlasmaComponents3.Label {
                            text: fullRoot.sparkChannelNowText()
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: fullRoot.channelColor(fullRoot.sparkChannel)
                        }
                    }

                    // Channel selector buttons
                    Flow {
                        Layout.fillWidth: true
                        spacing: 4
                        Repeater {
                            model: [
                                { key: "temp", label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Temp"), objName: "sparkChannelTempButton" },
                                { key: "cpu", label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU"), objName: "sparkChannelCpuButton" },
                                { key: "psi", label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Responsiveness"), objName: "sparkChannelPsiButton" },
                                { key: "power", label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Power"), objName: "sparkChannelPowerButton" },
                                { key: "battery", label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Battery"), objName: "sparkChannelBatteryButton" }
                            ]
                            delegate: PlasmaComponents3.ToolButton {
                                id: sparkBtn
                                required property var modelData
                                objectName: sparkBtn.modelData.objName
                                text: sparkBtn.modelData.label
                                checkable: true
                                checked: fullRoot.sparkChannel === sparkBtn.modelData.key
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Show ") + sparkBtn.modelData.label + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " sparkline")
                                onClicked: {
                                    fullRoot.sparkChannel = sparkBtn.modelData.key
                                    miniSparkCanvas.requestPaint()
                                }
                            }
                        }
                    }

                    // Sparkline Canvas
                    Canvas {
                        id: miniSparkCanvas
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 3
                        Accessible.role: Accessible.Graphic
                        Accessible.name: fullRoot.sparkChannelDescription()
                        Accessible.description: fullRoot.sparkChannelNowText()

                        Connections {
                            target: fullRoot.controller
                            function onTelemetryUpdated() {
                                miniSparkCanvas.requestPaint()
                            }
                        }

                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            const samples = fullRoot.controller.historySamples
                            if (!samples || samples.length < 2) return

                            const firstTime = samples[0].monotonicMs
                            const duration = Math.max(1, samples[samples.length - 1].monotonicMs - firstTime)
                            const ch = fullRoot.sparkChannel
                            const accent = fullRoot.channelColor(ch)

                            const powerKey = ch === "battery" ? "batteryWatts" : "packageWatts"
                            const powerAvailableKey = ch === "battery" ? "batteryDischargeAvailable" : "packagePowerAvailable"
                            let maxPower = 45.0
                            if (ch === "power" || ch === "battery") {
                                for (let m = 0; m < samples.length; ++m) {
                                    if (samples[m][powerAvailableKey] && samples[m][powerKey] > maxPower)
                                        maxPower = samples[m][powerKey] * 1.15
                                }
                            }

                            const segments = []
                            let points = []
                            for (let i = 0; i < samples.length; ++i) {
                                const sample = samples[i]
                                const available = ch === "cpu" ? sample.cpuLoadAvailable
                                                : ch === "psi" ? sample.psiAvailable
                                                : ch === "power" || ch === "battery" ? sample[powerAvailableKey]
                                                : sample.cpuTempAvailable
                                if ((!available || sample.gapBefore) && points.length > 0) {
                                    segments.push(points)
                                    points = []
                                }
                                if (!available) continue
                                let norm = 0.0
                                if (ch === "cpu") norm = sample.cpuLoad / 100.0
                                else if (ch === "psi") norm = sample.responsivenessScore / 100.0
                                else if (ch === "power" || ch === "battery") norm = sample[powerKey] / maxPower
                                else norm = (sample.cpuTemp - 30) / 70
                                norm = Math.min(1, Math.max(0, norm))
                                const x = (sample.monotonicMs - firstTime) / duration * width
                                const y = height - (norm * (height - 8)) - 4
                                points.push({ x: x, y: y })
                            }
                            if (points.length) segments.push(points)
                            ctx.lineWidth = 2
                            ctx.strokeStyle = accent.toString()
                            for (let segment of segments) {
                                if (segment.length < 2) continue
                                ctx.beginPath()
                                ctx.moveTo(segment[0].x, segment[0].y)
                                for (let point = 1; point < segment.length; ++point)
                                    ctx.lineTo(segment[point].x, segment[point].y)
                                ctx.stroke()
                            }
                        }
                    }
                }
            }

            // ==========================================
            // 5. POWER PROFILE QUICK SELECTOR
            // ==========================================
            RowLayout {
                visible: fullRoot.controller.powerProfileServiceAvailable && fullRoot.controller.availablePowerProfiles.length > 0
                Layout.fillWidth: true
                spacing: 6

                PlasmaComponents3.Label {
                    text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Profile:")
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    font.bold: true
                    opacity: 0.8
                }

                Repeater {
                    model: fullRoot.controller.availablePowerProfiles
                    delegate: PlasmaComponents3.Button {
                        id: pBtn
                        required property string modelData
                        text: pBtn.modelData
                        checkable: true
                        checked: fullRoot.controller.activePowerProfile === pBtn.modelData
                        enabled: !fullRoot.controller.powerProfileChangePending
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Set profile ") + pBtn.modelData
                        onClicked: fullRoot.controller.setPowerProfile(pBtn.modelData)
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // ==========================================
            // 6. RECENT INCIDENTS (COMPACT, GROUPED)
            // ==========================================
            ColumnLayout {
                Layout.fillWidth: true
                visible: (fullRoot.controller.groupedIncidentHistory && fullRoot.controller.groupedIncidentHistory.length > 0)
                         || (fullRoot.controller.incidentHistory && fullRoot.controller.incidentHistory.length > 0)
                spacing: 4

                PlasmaComponents3.Label {
                    text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Recent Incidents")
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    font.bold: true
                    opacity: 0.8
                }

                Repeater {
                    model: (fullRoot.controller.groupedIncidentHistory && fullRoot.controller.groupedIncidentHistory.length > 0)
                           ? fullRoot.controller.groupedIncidentHistory.slice(-2).reverse()
                           : fullRoot.controller.incidentHistory.slice(-2).reverse()
                    delegate: PlasmaComponents3.Button {
                        id: incBtn
                        required property var modelData
                        Layout.fillWidth: true
                        text: (incBtn.modelData.count > 1 ? ("[" + incBtn.modelData.count + "×] ") : "")
                              + (incBtn.modelData.latestTimeString || incBtn.modelData.timeString)
                              + " · " + incBtn.modelData.headline
                        icon.name: incBtn.modelData.severity >= 2 || incBtn.modelData.peakSeverity >= 2 ? "dialog-warning" : "dialog-information"
                        Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Investigate incident: ") + text
                        onClicked: {
                            var id = incBtn.modelData.incidentId || (incBtn.modelData.incidentIds ? incBtn.modelData.incidentIds[0] : "")
                            fullRoot.controller.launchIncident(id)
                        }
                    }
                }
            }

            // Measurement details collapsible expander
            PlasmaComponents3.Button {
                id: qualityToggle
                Layout.fillWidth: true
                icon.name: qualityToggle.checked ? "collapse-all" : "expand-all"
                text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Measurement details")
                checkable: true
                Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Show measurement details, sources, and sample times")
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: qualityToggle.checked
                spacing: Kirigami.Units.smallSpacing
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.qualityText("cpuLoad", KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU load"))
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.qualityText("cpuTemperature", KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU temperature"))
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.qualityText("pressureIndex", KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Responsiveness index"))
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.qualityText("packagePower", KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU package power"))
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
            }
        }
    }
}
