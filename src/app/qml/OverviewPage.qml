// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0
import "MeasurementLabels.js" as MeasurementLabels

Kirigami.ScrollablePage {
    id: page
    objectName: "overviewPage"
    title: KI18n.i18nd("loofi-pulse", "Overview")
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None
    titleDelegate: Item {
        width: parent ? parent.width : 0
        height: 0
        visible: false
    }
    topPadding: Kirigami.Units.largeSpacing
    bottomPadding: Kirigami.Units.largeSpacing
    leftPadding: Kirigami.Units.largeSpacing
    rightPadding: Kirigami.Units.largeSpacing

    required property var controller

    property bool detailsExpanded: false

    signal navigateRequested(string pageName, string section)

    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: page.Kirigami.Theme.backgroundColor
        disabledColor: page.Kirigami.Theme.disabledTextColor
        negativeColor: page.Kirigami.Theme.negativeTextColor
        neutralColor: page.Kirigami.Theme.neutralTextColor
        positiveColor: page.Kirigami.Theme.positiveTextColor
        textColor: page.Kirigami.Theme.textColor
        highlightColor: page.Kirigami.Theme.highlightColor
    }

    function focusSection(section) {
        if (!section) return true
        if (section === "measurements") page.detailsExpanded = true
        var target = (section === "measurements" && detailsCol) ? detailsCol : mainCol
        if (!target) return false
        Qt.callLater(function() {
            var position = target.mapToItem(page.flickable.contentItem, 0, 0)
            page.flickable.contentY = Math.max(0, Math.min(position.y - 12,
                page.flickable.contentHeight - page.flickable.height))
            target.forceActiveFocus(Qt.TabFocusReason)
        })
        return true
    }

    ColumnLayout {
        id: mainCol
        width: page.width - Kirigami.Units.largeSpacing * 2
        spacing: Kirigami.Units.largeSpacing

        // ==========================================
        // 1. PAGE HEADER ROW
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                QQC2.Label {
                    text: KI18n.i18nd("loofi-pulse", "Overview")
                    font.weight: Font.DemiBold
                    font.pointSize: page.theme.fontHeadline
                    color: page.theme.textColor
                }

                QQC2.Label {
                    text: KI18n.i18nd("loofi-pulse", "Real-time system health, performance, and hardware telemetry.")
                    font.pointSize: page.theme.fontCaption
                    color: page.theme.mutedTextColor
                }
            }

            // Health Status Badge
            Rectangle {
                radius: page.theme.pillRadius
                color: page.theme.severityBgColor(page.controller.verdictSeverity)
                border.width: 1
                border.color: page.theme.severityBorderColor(page.controller.verdictSeverity)
                implicitHeight: statusRow.implicitHeight + page.theme.spacingS
                implicitWidth: statusRow.implicitWidth + page.theme.spacingM

                RowLayout {
                    id: statusRow
                    anchors.centerIn: parent
                    spacing: 6

                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: page.theme.severityColor(page.controller.verdictSeverity)
                    }

                    ColumnLayout {
                        spacing: 0
                        QQC2.Label {
                            text: page.theme.severityBadgeText(page.controller.verdictSeverity)
                            color: page.theme.severityColor(page.controller.verdictSeverity)
                            font.bold: true
                            font.pointSize: page.theme.fontCaption
                        }
                        QQC2.Label {
                            text: KI18n.i18nd("loofi-pulse", page.controller.verdictHeadline)
                            color: page.theme.mutedTextColor
                            font.pointSize: page.theme.fontCaption * 0.9
                            elide: Text.ElideRight
                            Layout.maximumWidth: 180
                        }
                    }
                }

                QQC2.AbstractButton {
                    anchors.fill: parent
                    Accessible.name: KI18n.i18nd("loofi-pulse", "Open status details")
                    onClicked: page.navigateRequested("pressure", "overview")
                }
            }

            // Live State / Polling Control Pill
            Rectangle {
                radius: page.theme.pillRadius
                color: page.theme.surfaceColor
                border.width: 1
                border.color: page.theme.cardBorderColor
                implicitHeight: liveRow.implicitHeight + page.theme.spacingS
                implicitWidth: liveRow.implicitWidth + page.theme.spacingM

                RowLayout {
                    id: liveRow
                    anchors.centerIn: parent
                    spacing: 6

                    Kirigami.Icon {
                        source: page.controller.recordingPaused ? "media-playback-start" : "media-playback-pause"
                        implicitWidth: 14
                        implicitHeight: 14
                        color: page.theme.highlightColor
                    }

                    QQC2.Label {
                        text: page.controller.recordingPaused
                              ? KI18n.i18nd("loofi-pulse", "Paused")
                              : (page.controller.collectorState + " · " + page.controller.pollIntervalMs + " ms")
                        font.pointSize: page.theme.fontCaption
                        font.weight: Font.Medium
                        color: page.theme.textColor
                    }
                }

                QQC2.AbstractButton {
                    anchors.fill: parent
                    Accessible.name: KI18n.i18nd("loofi-pulse", "Toggle live recording")
                    onClicked: page.controller.recordingPaused = !page.controller.recordingPaused
                }
            }
        }

        // ==========================================
        // 2. REAL-TIME ACTIVITY CHART CARD
        // ==========================================
        PulseCard {
            Layout.fillWidth: true
            implicitHeight: chartCol.implicitHeight + padding * 2
            padding: page.theme.spacingM

            ColumnLayout {
                id: chartCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: page.theme.spacingS

                // Chart Header & Legend
                RowLayout {
                    Layout.fillWidth: true
                    spacing: page.theme.spacingM

                    RowLayout {
                        spacing: 6
                        Kirigami.Icon {
                            source: "view-statistics"
                            implicitWidth: 16
                            implicitHeight: 16
                            color: page.theme.cpuAccent
                        }
                        QQC2.Label {
                            text: KI18n.i18nd("loofi-pulse", "System Load & Temperature (Real-time)")
                            font.weight: Font.DemiBold
                            font.pointSize: page.theme.fontTitle
                            color: page.theme.textColor
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Legend: CPU Load
                    RowLayout {
                        spacing: 6
                        Rectangle { width: 10; height: 3; radius: 1.5; color: page.theme.cpuAccent }
                        QQC2.Label {
                            text: KI18n.i18nd("loofi-pulse", "CPU Load (%)")
                            font.pointSize: page.theme.fontCaption
                            color: page.theme.mutedTextColor
                        }
                    }

                    // Legend: CPU Temperature
                    RowLayout {
                        spacing: 6
                        Rectangle { width: 10; height: 3; radius: 1.5; color: page.theme.tempAccent }
                        QQC2.Label {
                            text: KI18n.i18nd("loofi-pulse", "CPU Temp (°C)")
                            font.pointSize: page.theme.fontCaption
                            color: page.theme.mutedTextColor
                        }
                    }
                }

                // Chart Canvas Area
                Item {
                    id: chartArea
                    Layout.fillWidth: true
                    Layout.preferredHeight: 130

                    readonly property var cpuLoadSeries: {
                        var json = page.controller.historyRecentSeriesJson("cpuLoadPercent", "cpuLoadAvailable", 50)
                        if (!json) return []
                        try {
                            var parsed = JSON.parse(json)
                            var res = []
                            for (var i = 0; i < parsed.length; ++i) {
                                if (parsed[i].available && isFinite(parsed[i].value)) res.push(parsed[i].value)
                            }
                            return res
                        } catch (e) {
                            return []
                        }
                    }

                    readonly property var cpuTempSeries: {
                        var json = page.controller.historyRecentSeriesJson("cpuPackageTemp", "cpuTempAvailable", 50)
                        if (!json) return []
                        try {
                            var parsed = JSON.parse(json)
                            var res = []
                            for (var i = 0; i < parsed.length; ++i) {
                                if (parsed[i].available && isFinite(parsed[i].value)) res.push(parsed[i].value)
                            }
                            return res
                        } catch (e) {
                            return []
                        }
                    }

                    readonly property bool hasTelemetry: cpuLoadSeries.length > 1 || cpuTempSeries.length > 1

                    // Horizontal Gridlines
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 28
                        anchors.bottomMargin: 20
                        spacing: 0

                        Repeater {
                            model: 4
                            delegate: Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: page.theme.subtleBorderColor
                                }
                            }
                        }
                    }

                    // Y-Axis Labels
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 20
                        width: 24
                        spacing: 0

                        QQC2.Label { text: "100"; font.pointSize: page.theme.fontCaption * 0.85; color: page.theme.faintTextColor }
                        Item { Layout.fillHeight: true }
                        QQC2.Label { text: "75"; font.pointSize: page.theme.fontCaption * 0.85; color: page.theme.faintTextColor }
                        Item { Layout.fillHeight: true }
                        QQC2.Label { text: "50"; font.pointSize: page.theme.fontCaption * 0.85; color: page.theme.faintTextColor }
                        Item { Layout.fillHeight: true }
                        QQC2.Label { text: "25"; font.pointSize: page.theme.fontCaption * 0.85; color: page.theme.faintTextColor }
                        Item { Layout.fillHeight: true }
                        QQC2.Label { text: "0"; font.pointSize: page.theme.fontCaption * 0.85; color: page.theme.faintTextColor }
                    }

                    // Canvas Line Drawing
                    Canvas {
                        id: realTimeCanvas
                        anchors.fill: parent
                        anchors.leftMargin: 28
                        anchors.bottomMargin: 20

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)

                            if (!chartArea.hasTelemetry) return

                            function renderLine(vals, strokeColor) {
                                if (!vals || vals.length < 2) return
                                ctx.strokeStyle = strokeColor
                                ctx.lineWidth = 2
                                ctx.lineJoin = "round"
                                ctx.beginPath()
                                var step = width / (vals.length - 1)
                                for (var i = 0; i < vals.length; ++i) {
                                    var v = Math.max(0, Math.min(100, Number(vals[i]) || 0))
                                    var y = height - (v / 100 * height)
                                    var x = i * step
                                    if (i === 0) ctx.moveTo(x, y)
                                    else ctx.lineTo(x, y)
                                }
                                ctx.stroke()
                            }

                            renderLine(chartArea.cpuTempSeries, page.theme.tempAccent)
                            renderLine(chartArea.cpuLoadSeries, page.theme.cpuAccent)
                        }

                        Connections {
                            target: page.controller
                            function onTelemetryUpdated() { realTimeCanvas.requestPaint() }
                        }
                    }

                    // Empty state when no data yet
                    QQC2.Label {
                        anchors.centerIn: parent
                        visible: !chartArea.hasTelemetry
                        text: KI18n.i18nd("loofi-pulse", "Awaiting telemetry samples…")
                        font.pointSize: page.theme.fontBody
                        color: page.theme.faintTextColor
                    }

                    // X-Axis Time Labels
                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 28
                        height: 18

                        QQC2.Label { text: "-5m"; font.pointSize: page.theme.fontCaption * 0.85; color: page.theme.faintTextColor }
                        Item { Layout.fillWidth: true }
                        QQC2.Label { text: "-4m"; font.pointSize: page.theme.fontCaption * 0.85; color: page.theme.faintTextColor }
                        Item { Layout.fillWidth: true }
                        QQC2.Label { text: "-3m"; font.pointSize: page.theme.fontCaption * 0.85; color: page.theme.faintTextColor }
                        Item { Layout.fillWidth: true }
                        QQC2.Label { text: "-2m"; font.pointSize: page.theme.fontCaption * 0.85; color: page.theme.faintTextColor }
                        Item { Layout.fillWidth: true }
                        QQC2.Label { text: "-1m"; font.pointSize: page.theme.fontCaption * 0.85; color: page.theme.faintTextColor }
                        Item { Layout.fillWidth: true }
                        QQC2.Label { text: KI18n.i18nd("loofi-pulse", "Now"); font.pointSize: page.theme.fontCaption * 0.85; color: page.theme.faintTextColor }
                    }
                }
            }
        }

        // ==========================================
        // 3. FIVE CORE SYSTEM PILLARS (GRID)
        // ==========================================
        GridLayout {
            Layout.fillWidth: true
            columns: page.width < 750 ? 2 : (page.width < 1100 ? 3 : 5)
            rowSpacing: page.theme.spacingS
            columnSpacing: page.theme.spacingS

            // 1. CPU
            MetricCard {
                Layout.fillWidth: true
                title: "CPU"
                available: page.controller.cpuLoadAvailable
                value: page.controller.cpuLoadAvailable ? Math.round(page.controller.cpuLoadPercent).toString() : "—"
                unit: "%"
                caption: page.controller.cpuTempAvailable
                         ? (page.controller.cpuPackageTemp.toFixed(0) + " °C · " + (page.controller.cpuAvgFreqMhz > 0 ? page.controller.cpuAvgFreqMhz + " MHz" : ""))
                         : KI18n.i18nd("loofi-pulse", "Package temp unavailable")
                channelColor: page.theme.cpuAccent
                trend: {
                    var raw = page.controller.historyRecentSeriesJson("cpuLoadPercent", "cpuLoadAvailable", 16)
                    try { return JSON.parse(raw).map(function(item) { return item.value || 0 }) } catch (e) { return [] }
                }
                trendMin: 0
                trendMax: 100
                onClicked: page.navigateRequested("pressure", "cpu")
            }

            // 2. RAM & Memory
            MetricCard {
                Layout.fillWidth: true
                title: KI18n.i18nd("loofi-pulse", "Memory")
                available: page.controller.memoryAvailable
                value: page.controller.memoryAvailable ? Math.round(page.controller.ramUsedPercent).toString() : "—"
                unit: "%"
                caption: page.controller.memoryAvailable
                         ? (page.controller.ramUsedGib.toFixed(1) + " / " + page.controller.ramTotalGib.toFixed(1) + " GiB")
                         : KI18n.i18nd("loofi-pulse", "RAM unavailable")
                channelColor: page.theme.psiAccent
                trend: {
                    var raw = page.controller.historyRecentSeriesJson("ramUsedPercent", "ramAvailable", 16)
                    try { return JSON.parse(raw).map(function(item) { return item.value || 0 }) } catch (e) { return [] }
                }
                trendMin: 0
                trendMax: 100
                onClicked: page.navigateRequested("pressure", "memory")
            }

            // 3. Systemrespons (PSI)
            MetricCard {
                Layout.fillWidth: true
                title: KI18n.i18nd("loofi-pulse", "Responsiveness")
                available: page.controller.psiAvailable
                value: page.controller.psiAvailable ? Math.round(page.controller.responsivenessScore).toString() : "—"
                unit: "/ 100"
                caption: page.controller.psiCpuSome10 > 0.5
                         ? ("CPU-stall: " + page.controller.psiCpuSome10.toFixed(1) + "%")
                         : KI18n.i18nd("loofi-pulse", "No pressure detected")
                channelColor: page.theme.channelColor("psi")
                trend: {
                    var raw = page.controller.historyRecentSeriesJson("responsivenessScore", "psiAvailable", 16)
                    try { return JSON.parse(raw).map(function(item) { return item.value || 0 }) } catch (e) { return [] }
                }
                trendMin: 0
                trendMax: 100
                onClicked: page.navigateRequested("pressure", "overview")
            }

            // 4. GPU
            MetricCard {
                Layout.fillWidth: true
                title: "GPU"
                available: page.controller.gpuAvailable && (page.controller.gpuBusyAvailable || page.controller.gpuTempAvailable)
                value: page.controller.gpuBusyAvailable
                       ? Math.round(page.controller.gpuBusyPercent).toString()
                       : (page.controller.gpuTempAvailable ? page.controller.gpuTempCelsius.toFixed(0) : "—")
                unit: page.controller.gpuBusyAvailable ? "%" : (page.controller.gpuTempAvailable ? "°C" : "")
                caption: page.controller.gpuAvailable
                         ? (page.controller.gpuDriverName.length ? page.controller.gpuDriverName : KI18n.i18nd("loofi-pulse", "Active"))
                         : KI18n.i18nd("loofi-pulse", "No discrete GPU")
                channelColor: page.theme.gpuAccent
                trend: {
                    var raw = page.controller.historyRecentSeriesJson("gpuBusyPercent", "gpuBusyAvailable", 16)
                    try { return JSON.parse(raw).map(function(item) { return item.value || 0 }) } catch (e) { return [] }
                }
                trendMin: 0
                trendMax: 100
                onClicked: page.navigateRequested("power", "gpu")
            }

            // 5. Energi & Batteri
            MetricCard {
                Layout.fillWidth: true
                title: page.controller.batteryPresent
                       ? KI18n.i18nd("loofi-pulse", "Power & Battery")
                       : KI18n.i18nd("loofi-pulse", "Power Consumption")
                available: page.controller.packagePowerAvailable || page.controller.batteryLevelAvailable
                value: page.controller.packagePowerAvailable
                       ? page.controller.packageWatts.toFixed(1)
                       : (page.controller.batteryLevelAvailable ? Math.round(page.controller.batteryPercent).toString() : "—")
                unit: page.controller.packagePowerAvailable ? "W" : (page.controller.batteryLevelAvailable ? "%" : "")
                caption: page.controller.batteryPresent
                         ? (Math.round(page.controller.batteryPercent) + "% · " + (page.controller.batteryStatus ? page.controller.batteryStatus : KI18n.i18nd("loofi-pulse", "AC Power")))
                         : (page.controller.onAcPower ? KI18n.i18nd("loofi-pulse", "Connected to AC") : KI18n.i18nd("loofi-pulse", "Standard Power"))
                channelColor: page.theme.powerAccent
                trend: {
                    var raw = page.controller.historyRecentSeriesJson("packageWatts", "packagePowerAvailable", 16)
                    try { return JSON.parse(raw).map(function(item) { return item.value || 0 }) } catch (e) { return [] }
                }
                trendMin: 0
                trendMax: page.controller.historyMaxWatts > 0 ? page.controller.historyMaxWatts : 65
                onClicked: page.navigateRequested("power", page.controller.batteryPresent ? "battery" : "cpu")
            }
        }

        // ==========================================
        // 4. LOWER TWO-COLUMN DETAILS SECTION
        // ==========================================
        GridLayout {
            Layout.fillWidth: true
            columns: page.width < 800 ? 1 : 2
            rowSpacing: page.theme.spacingM
            columnSpacing: page.theme.spacingM

            // ------------------------------------------
            // LEFT COLUMN: Processor & Hårdvarusensorer
            // ------------------------------------------
            PulseCard {
                Layout.fillWidth: true
                implicitHeight: leftColContent.implicitHeight + padding * 2
                padding: page.theme.spacingM

                ColumnLayout {
                    id: leftColContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: page.theme.spacingM

                    // Card Title
                    RowLayout {
                        spacing: 6
                        Kirigami.Icon {
                            source: "preferences-devices-cpu"
                            implicitWidth: 16
                            implicitHeight: 16
                            color: page.theme.cpuAccent
                        }
                        QQC2.Label {
                            text: KI18n.i18nd("loofi-pulse", "CPU Cores & Hardware Sensors")
                            font.weight: Font.DemiBold
                            font.pointSize: page.theme.fontTitle
                            color: page.theme.textColor
                        }
                    }

                    // Per-Core Tiles (if available)
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        rowSpacing: 6
                        columnSpacing: 6
                        visible: page.controller.coreItems && page.controller.coreItems.length > 0

                        Repeater {
                            model: (page.controller.coreItems && page.controller.coreItems.length > 0)
                                   ? page.controller.coreItems.slice(0, 8) : []
                            delegate: Rectangle {
                                id: coreTile
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 48
                                radius: page.theme.innerRadius
                                color: page.theme.cardNestedBg
                                border.width: 1
                                border.color: page.theme.subtleBorderColor

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 2

                                    RowLayout {
                                        Layout.fillWidth: true
                                        QQC2.Label {
                                            text: "Core " + coreTile.modelData.coreId
                                            font.weight: Font.Medium
                                            font.pointSize: page.theme.fontCaption
                                            color: page.theme.textColor
                                        }
                                        Item { Layout.fillWidth: true }
                                        QQC2.Label {
                                            text: coreTile.modelData.tempCelsius > 0
                                                  ? coreTile.modelData.tempCelsius.toFixed(0) + " °C" : "—"
                                            font.pointSize: page.theme.fontCaption * 0.9
                                            color: page.theme.tempAccent
                                        }
                                    }

                                    QQC2.Label {
                                        text: coreTile.modelData.loadPercent > 0
                                              ? Math.round(coreTile.modelData.loadPercent) + " %" : "0 %"
                                        font.pointSize: page.theme.fontCaption
                                        color: page.theme.mutedTextColor
                                    }
                                }
                            }
                        }
                    }

                    // Hardware Sensors List
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: page.theme.spacingS

                        QQC2.Label {
                            text: KI18n.i18nd("loofi-pulse", "Thermal Sensors")
                            font.weight: Font.Medium
                            font.pointSize: page.theme.fontBody
                            color: page.theme.textColor
                        }

                        // CPU Package Temp Row
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: page.theme.spacingM
                            Kirigami.Icon { source: "preferences-devices-cpu"; implicitWidth: 14; implicitHeight: 14; color: page.theme.mutedTextColor }
                            QQC2.Label {
                                text: "CPU Package"
                                font.pointSize: page.theme.fontCaption
                                color: page.theme.textColor
                                Layout.preferredWidth: 120
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 5
                                radius: 2.5
                                color: page.theme.trackColor
                                Rectangle {
                                    width: parent.width * Math.max(0.02, Math.min(1.0, (page.controller.cpuPackageTemp || 0) / 100))
                                    height: parent.height
                                    radius: 2.5
                                    color: page.theme.tempAccent
                                }
                            }
                            QQC2.Label {
                                text: page.controller.cpuTempAvailable ? page.controller.cpuPackageTemp.toFixed(0) + " °C" : "—"
                                font.weight: Font.Medium
                                font.pointSize: page.theme.fontCaption
                                color: page.theme.textColor
                                Layout.preferredWidth: 46
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        // NVMe Storage Temp Row (if available)
                        RowLayout {
                            visible: page.controller.nvmeTempAvailable
                            Layout.fillWidth: true
                            spacing: page.theme.spacingM
                            Kirigami.Icon { source: "drive-harddisk"; implicitWidth: 14; implicitHeight: 14; color: page.theme.mutedTextColor }
                            QQC2.Label {
                                text: "NVMe Storage"
                                font.pointSize: page.theme.fontCaption
                                color: page.theme.textColor
                                Layout.preferredWidth: 120
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 5
                                radius: 2.5
                                color: page.theme.trackColor
                                Rectangle {
                                    width: parent.width * Math.max(0.02, Math.min(1.0, (page.controller.nvmeTempCelsius || 0) / 100))
                                    height: parent.height
                                    radius: 2.5
                                    color: page.theme.storageAccent
                                }
                            }
                            QQC2.Label {
                                text: page.controller.nvmeTempAvailable ? page.controller.nvmeTempCelsius.toFixed(0) + " °C" : "—"
                                font.weight: Font.Medium
                                font.pointSize: page.theme.fontCaption
                                color: page.theme.textColor
                                Layout.preferredWidth: 46
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        // Wi-Fi Network Temp / Signal Row (if available)
                        RowLayout {
                            visible: page.controller.wifiAvailable
                            Layout.fillWidth: true
                            spacing: page.theme.spacingM
                            Kirigami.Icon { source: "network-wireless"; implicitWidth: 14; implicitHeight: 14; color: page.theme.mutedTextColor }
                            QQC2.Label {
                                text: page.controller.wifiSsid.length > 0 ? page.controller.wifiSsid : KI18n.i18nd("loofi-pulse", "Wi-Fi Interface")
                                font.pointSize: page.theme.fontCaption
                                color: page.theme.textColor
                                Layout.preferredWidth: 120
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 5
                                radius: 2.5
                                color: page.theme.trackColor
                                Rectangle {
                                    width: parent.width * Math.max(0.02, Math.min(1.0, (page.controller.wifiSignalPercent || 0) / 100))
                                    height: parent.height
                                    radius: 2.5
                                    color: page.theme.psiAccent
                                }
                            }
                            QQC2.Label {
                                text: page.controller.wifiSignalPercent + " %"
                                font.weight: Font.Medium
                                font.pointSize: page.theme.fontCaption
                                color: page.theme.textColor
                                Layout.preferredWidth: 46
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }
            }

            // ------------------------------------------
            // RIGHT COLUMN: Energiprofil & Diagnostik
            // ------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.theme.spacingM

                // Power Profile Selector Card
                PulseCard {
                    Layout.fillWidth: true
                    implicitHeight: profCol.implicitHeight + padding * 2
                    padding: page.theme.spacingM

                    ColumnLayout {
                        id: profCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: page.theme.spacingS

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Kirigami.Icon {
                                source: "configure-shortcuts"
                                implicitWidth: 16
                                implicitHeight: 16
                                color: page.theme.powerAccent
                            }
                            QQC2.Label {
                                text: KI18n.i18nd("loofi-pulse", "Power Profile")
                                font.weight: Font.DemiBold
                                font.pointSize: page.theme.fontTitle
                                color: page.theme.textColor
                            }
                            Item { Layout.fillWidth: true }
                            QQC2.Label {
                                text: page.controller.activePowerProfile.length > 0
                                      ? page.controller.activePowerProfile
                                      : KI18n.i18nd("loofi-pulse", "Default")
                                font.pointSize: page.theme.fontCaption
                                font.weight: Font.Medium
                                color: page.theme.highlightColor
                            }
                        }

                        // Profile Buttons
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: page.theme.spacingS

                            Repeater {
                                model: page.controller.availablePowerProfiles.length > 0
                                       ? page.controller.availablePowerProfiles
                                       : ["power-saver", "balanced", "performance"]
                                delegate: Rectangle {
                                    id: profBtn
                                    required property string modelData
                                    readonly property bool isSelected: page.controller.activePowerProfile === profBtn.modelData
                                    Layout.fillWidth: true
                                    implicitHeight: 44
                                    radius: page.theme.innerRadius
                                    color: isSelected ? page.theme.selectionColor : (profHover.hovered ? page.theme.subtleSurfaceColor : page.theme.cardNestedBg)
                                    border.width: isSelected ? 1 : 0
                                    border.color: isSelected ? page.theme.highlightColor : "transparent"

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Kirigami.Icon {
                                            source: profBtn.modelData === "power-saver" ? "battery-profile-powersave"
                                                  : (profBtn.modelData === "performance" ? "battery-profile-performance" : "battery-profile-balanced")
                                            implicitWidth: 14
                                            implicitHeight: 14
                                            color: profBtn.isSelected ? page.theme.highlightColor : page.theme.mutedTextColor
                                        }
                                        QQC2.Label {
                                            text: profBtn.modelData
                                            font.weight: profBtn.isSelected ? Font.Medium : Font.Normal
                                            font.pointSize: page.theme.fontCaption
                                            color: profBtn.isSelected ? page.theme.textColor : page.theme.mutedTextColor
                                        }
                                    }

                                    HoverHandler { id: profHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler {
                                        onTapped: {
                                            if (page.controller.powerProfileServiceAvailable) {
                                                page.controller.setPowerProfile(profBtn.modelData)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // System Health & Diagnostics Summary Card
                PulseCard {
                    Layout.fillWidth: true
                    implicitHeight: diagCol.implicitHeight + padding * 2
                    padding: page.theme.spacingM

                    ColumnLayout {
                        id: diagCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: page.theme.spacingS

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Kirigami.Icon {
                                source: page.theme.severityIcon(page.controller.verdictSeverity)
                                implicitWidth: 16
                                implicitHeight: 16
                                color: page.theme.severityColor(page.controller.verdictSeverity)
                            }
                            QQC2.Label {
                                text: KI18n.i18nd("loofi-pulse", "Diagnostic Status")
                                font.weight: Font.DemiBold
                                font.pointSize: page.theme.fontTitle
                                color: page.theme.textColor
                            }
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: page.controller.verdictSeverity > 0
                                  ? (KI18n.i18nd("loofi-pulse", page.controller.verdictHeadline) + (page.controller.culpritName.length > 0 ? " — " + page.controller.culpritName : ""))
                                  : KI18n.i18nd("loofi-pulse", "All hardware sub-systems operating nominally without thermal or pressure bottlenecks.")
                            font.pointSize: page.theme.fontBody
                            color: page.controller.verdictSeverity > 0 ? page.theme.textColor : page.theme.mutedTextColor
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: page.controller.verdictSeverity > 0
                            spacing: page.theme.spacingS

                            PulseButton {
                                text: KI18n.i18nd("loofi-pulse", "Investigate Incident")
                                icon.name: "tools-report-bug"
                                onClicked: page.navigateRequested("pressure", "overview")
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 5. MEASUREMENT QUALITY & DETAILS
        // ==========================================
        PulseButton {
            id: measurementDetailsBtn
            objectName: "measurementDetailsButton"
            Layout.fillWidth: true
            icon.name: page.detailsExpanded ? "collapse-all" : "expand-all"
            text: page.detailsExpanded
                  ? KI18n.i18nd("loofi-pulse", "Hide measurement details")
                  : KI18n.i18nd("loofi-pulse", "Measurement details")
            checkable: true
            checked: page.detailsExpanded
            onToggled: page.detailsExpanded = checked
        }

        MeasurementDetails {
            id: detailsCol
            visible: page.detailsExpanded
            Layout.fillWidth: true
            metadata: page.controller ? page.controller.metricMetadata : null
            expanded: page.detailsExpanded
        }
    }
}
