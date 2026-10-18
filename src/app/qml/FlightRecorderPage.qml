pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "MeasurementLabels.js" as MeasurementLabels
import org.kde.kirigami as Kirigami
import QtCore as QtCore

Kirigami.ScrollablePage {
    id: flightPage
    objectName: "historyPage"
    title: "History"
    titleDelegate: Item {
        width: parent ? parent.width : 0
        height: 0
        visible: false
    }

    required property var controller
    signal saveSessionRequested()
    signal openSessionRequested()
    signal openSettingsRequested()
    property string activeViewMode: "flight"
    property var rangeStartId: 0
    property var rangeEndId: 0
    property double rangeStartTimeMs: 0
    property double rangeEndTimeMs: 0
    readonly property var historyInfo: controller.historySummary
    readonly property var inspectedDetail: controller.selectedHistoryDetail
    readonly property string rangeCaptureId: String(historyInfo.captureId || "")
    onRangeCaptureIdChanged: { rangeStartId = 0; rangeEndId = 0; rangeStartTimeMs = 0; rangeEndTimeMs = 0 }

    function comparisonText(window) {
        if (!window || !window.available) return "No observations available"
        return window.sampleCount + " observations · " + (Number(window.coverageMs) / 1000).toFixed(1) + " s coverage"
             + (window.complete ? " · Complete" : " · Partial coverage")
             + " · " + (window.gapCount || 0) + " gaps"
             + "\nCPU average: " + (window.cpuLoadAvailable ? Number(window.cpuLoadPercent).toFixed(1) + "%" : "Unavailable")
             + " · Responsiveness: " + (window.responsivenessAvailable ? Number(window.responsivenessScore).toFixed(1) + "/100" : "Unavailable")
    }


    function focusSection(section) {
        if (!section) return true
        var targets = {"timeline": timelineCanvas, "incidents": incCol, "inspector": inspectorCol}
        var target = targets[section]
        if (!target) return false

        Qt.callLater(function() {
            var position = target.mapToItem(flightPage.flickable.contentItem, 0, 0)
            flightPage.flickable.contentY = Math.max(0, Math.min(position.y - 12,
                flightPage.flickable.contentHeight - flightPage.flickable.height))
            target.forceActiveFocus(Qt.TabFocusReason)
        })
        return true
    }


    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: flightPage.Kirigami.Theme.backgroundColor
        disabledColor: flightPage.Kirigami.Theme.disabledTextColor
        negativeColor: flightPage.Kirigami.Theme.negativeTextColor
        neutralColor: flightPage.Kirigami.Theme.neutralTextColor
        positiveColor: flightPage.Kirigami.Theme.positiveTextColor
    }

    QtCore.Settings {
        id: historySettings
        category: "LoofiPulseHistory"
        property bool showTemp: true
        property bool showLoad: true
        property bool showPressure: true
        property bool showPower: false
        property bool showBatteryPower: false
        property bool showGpu: false
        property bool showDisk: false
    }

    property bool showTempTrace: historySettings.showTemp
    property bool showLoadTrace: historySettings.showLoad
    property bool showPsiTrace: historySettings.showPressure
    property bool showWattsTrace: historySettings.showPower
    property bool showBatteryTrace: historySettings.showBatteryPower
    onShowBatteryTraceChanged: historySettings.showBatteryPower = showBatteryTrace
    property bool showGpuTrace: historySettings.showGpu
    property bool showDiskTrace: historySettings.showDisk
    property int hoveredSampleIndex: -1

    onShowTempTraceChanged: historySettings.showTemp = showTempTrace
    onShowLoadTraceChanged: historySettings.showLoad = showLoadTrace
    onShowPsiTraceChanged: historySettings.showPressure = showPsiTrace
    onShowWattsTraceChanged: historySettings.showPower = showWattsTrace
    onShowGpuTraceChanged: historySettings.showGpu = showGpuTrace
    onShowDiskTraceChanged: historySettings.showDisk = showDiskTrace

    function selectStep(delta) {
        var count = historyInfo.count
        if (!count) return
        var index = controller.selectedHistoryIndex
        index = index < 0 ? count - 1 : Math.max(0, Math.min(count - 1, index + delta))
        controller.selectedHistorySampleId = controller.historySampleAt(index).sampleId
    }

    function yAxisLabelAt(fraction) {
        var parts = []
        if (flightPage.showLoadTrace || flightPage.showPsiTrace || flightPage.showGpuTrace) {
            parts.push(Math.round(fraction * 100) + "%")
        }
        if (flightPage.showTempTrace && (fraction === 0 || fraction === 0.5 || fraction === 1.0)) {
            parts.push(Math.round(25 + fraction * 75) + "°")
        }
        if ((flightPage.showWattsTrace || flightPage.showBatteryTrace) && (fraction === 0.5 || fraction === 1.0)) {
            parts.push(Math.round(fraction * Math.max(65, flightPage.controller.historyMaxWatts)) + "W")
        }
        if (flightPage.showDiskTrace && (fraction === 0.5 || fraction === 1.0)) {
            parts.push(Math.round(fraction * Math.max(50, flightPage.controller.historyMaxDiskMibPerSec)) + "M")
        }
        return parts.length > 0 ? parts.join(" · ") : (Math.round(fraction * 100) + "%")
    }

    function hoveredSampleText() {
        if (flightPage.hoveredSampleIndex < 0 || flightPage.hoveredSampleIndex >= flightPage.historyInfo.count) return ""
        var s = flightPage.controller.historySampleAt(flightPage.hoveredSampleIndex)
        var parts = ["Hover @ " + (s.timeString || "--:--:--")]
        if (s.cpuTempAvailable) parts.push("CPU " + Number(s.cpuTemp).toFixed(1) + " °C")
        if (s.cpuLoadAvailable) parts.push("Load " + Number(s.cpuLoad).toFixed(0) + "%")
        if (s.psiAvailable) parts.push("Responsiveness index " + Number(s.responsivenessScore).toFixed(1) + "/100")
        if (s.packagePowerAvailable) parts.push("Package " + Number(s.packageWatts).toFixed(1) + " W")
        if (s.batteryDischargeAvailable) parts.push("Battery discharge " + Number(s.batteryWatts).toFixed(1) + " W")
        if (s.diskThroughputAvailable) parts.push("Disk " + Number(s.diskReadMibPerSec + s.diskWriteMibPerSec).toFixed(1) + " MiB/s")
        parts.push("Click to freeze moment")
        return parts.join("  ·  ")
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        // Mode Selector: Flight Recorder vs Long-Term Trends
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            PulseButton {
                text: "Live Flight Recorder"
                icon.name: "media-record"
                checkable: true
                checked: flightPage.activeViewMode === "flight"
                onClicked: flightPage.activeViewMode = "flight"
            }

            PulseButton {
                text: "Long-Term Trends (1h – 30d)"
                icon.name: "view-history"
                checkable: true
                checked: flightPage.activeViewMode === "trends"
                onClicked: {
                    flightPage.activeViewMode = "trends"
                    flightPage.controller.loadTrendsRange(flightPage.controller.trendsRange || "1h")
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Long-Term Trends View
        TrendsView {
            Layout.fillWidth: true
            visible: flightPage.activeViewMode === "trends"
            controller: flightPage.controller
            theme: flightPage.theme
            onOpenSettingsRequested: flightPage.openSettingsRequested()
        }

        ColumnLayout {
            id: flightSubLayout
            Layout.fillWidth: true
            visible: flightPage.activeViewMode === "flight"
            spacing: Kirigami.Units.largeSpacing

            // 1. Telemetry Controls & Export Toolbar Card
            Rectangle {
            Layout.fillWidth: true
            implicitHeight: toolbarCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1
            border.color: theme.cardBorderColor

            ColumnLayout {
                id: toolbarCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                // Logical Toolbar Groups
                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    // 1. Playback cluster
                    RowLayout {
                        spacing: 4
                        PulseButton {
                            icon.name: "media-playback-start"
                            text: "Follow Live"
                            enabled: flightPage.controller.sessionMode || flightPage.controller.selectedHistorySampleId !== 0
                            onClicked: flightPage.controller.returnToLive()
                        }

                        PulseButton {
                            icon.name: flightPage.controller.recordingPaused ? "media-playback-start" : "media-playback-pause"
                            text: flightPage.controller.recordingPaused ? "Resume" : "Pause"
                            checkable: true
                            checked: flightPage.controller.recordingPaused
                            onClicked: flightPage.controller.recordingPaused = !flightPage.controller.recordingPaused
                        }
                    }

                    // 2. Incident & Marking cluster
                    RowLayout {
                        spacing: 4
                        PulseButton {
                            objectName: "markProblemNowButton"
                            text: "Mark problem now"
                            icon.name: "bookmark-new"
                            isPrimary: true
                            accentColor: theme.negativeColor
                            enabled: !flightPage.controller.sessionMode
                            onClicked: flightPage.controller.markProblemNow()
                        }
                    }

                    // 3. Sessions & Export cluster
                    RowLayout {
                        spacing: 4
                        PulseButton {
                            text: "Save session…"
                            icon.name: "document-save-as"
                            onClicked: flightPage.saveSessionRequested()
                        }
                        PulseButton {
                            text: "Open session…"
                            icon.name: "document-open"
                            onClicked: flightPage.openSessionRequested()
                        }
                        PulseButton {
                            icon.name: "document-save"
                            text: flightPage.controller.sessionMode ? "Export incident" : "Export recording"
                            onClicked: {
                                if (flightPage.controller.sessionMode) flightPage.controller.exportSelectedIncident()
                                else flightPage.controller.exportEntireRecording()
                            }
                        }
                        PulseButton {
                            text: "More"
                            icon.name: "overflow-menu"
                            onClicked: historyMenu.popup()
                            QQC2.Menu {
                                id: historyMenu
                                QQC2.MenuItem {
                                    text: "Peak thermal spike"
                                    icon.name: "temperature-normal"
                                    onTriggered: flightPage.controller.jumpToPeakThermalSpike()
                                }
                                QQC2.MenuItem {
                                    text: "Worst PSI wait"
                                    icon.name: "memory"
                                    onTriggered: flightPage.controller.jumpToWorstStallSpike()
                                }
                                QQC2.MenuItem {
                                    text: "Copy incident report"
                                    icon.name: "edit-copy"
                                    onTriggered: flightPage.controller.copyIncidentReportToClipboard()
                                }
                                QQC2.MenuItem {
                                    text: "Open reports folder"
                                    icon.name: "folder-open"
                                    enabled: flightPage.controller.lastActionPath.length > 0
                                    onTriggered: flightPage.controller.openReportsFolder()
                                }
                                QQC2.MenuSeparator {}
                                QQC2.MenuItem {
                                    objectName: "clearHistoryButton"
                                    text: "Clear recorded history"
                                    icon.name: "edit-clear-all"
                                    enabled: flightPage.historyInfo.count > 0
                                    onTriggered: clearHistoryDialog.open()
                                }
                            }
                        }
                    }

                    QQC2.Dialog {
                        id: clearHistoryDialog
                        title: "Clear recorded history?"
                        modal: true
                        standardButtons: QQC2.Dialog.Cancel | QQC2.Dialog.Ok
                        onAccepted: flightPage.controller.clearHistory()
                        QQC2.Label { text: "This clears the current in-memory recording and incidents."; wrapMode: Text.WordWrap }
                    }

                    // Recording Info Pill
                    Rectangle {
                        radius: theme.pillRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        implicitWidth: sampleCountLbl.implicitWidth + 14
                        implicitHeight: sampleCountLbl.implicitHeight + 8

                        QQC2.Label {
                            id: sampleCountLbl
                            anchors.centerIn: parent
                            text: flightPage.historyInfo.count + " samples · "
                                  + flightPage.controller.actualHistorySeconds.toFixed(0) + " s / "
                                  + Math.round(flightPage.controller.historyRetentionSeconds / 60) + " min"
                            color: Kirigami.Theme.disabledTextColor
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }
                }

                // Channel Toggle Chips
                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: [
                            {
                                key: "temp",
                                label: "CPU temp (25–100 °C)",
                                accentKey: "temp",
                                active: flightPage.showTempTrace
                            },
                            {
                                key: "load",
                                label: "CPU load (0–100%)",
                                accentKey: "cpu",
                                active: flightPage.showLoadTrace
                            },
                            {
                                key: "psi",
                                label: "Responsiveness (0–100)",
                                accentKey: "psi",
                                active: flightPage.showPsiTrace
                            },
                            {
                                key: "watts",
                                label: "Power (0–" + Math.round(flightPage.controller.historyMaxWatts) + " W)",
                                accentKey: "power",
                                active: flightPage.showWattsTrace
                            },
                            {
                                key: "battery",
                                label: "Battery (0–" + Math.round(flightPage.controller.historyMaxWatts) + " W)",
                                accentKey: "battery",
                                active: flightPage.showBatteryTrace
                            },
                            {
                                key: "gpu",
                                label: "GPU load (0–100%)",
                                accentKey: "gpu",
                                active: flightPage.showGpuTrace
                            },
                            {
                                key: "disk",
                                label: "Disk I/O (0–" + Math.round(flightPage.controller.historyMaxDiskMibPerSec) + " MiB/s)",
                                accentKey: "storage",
                                active: flightPage.showDiskTrace
                            }
                        ]

                        delegate: QQC2.AbstractButton {
                            id: channelChip
                            required property var modelData
                            readonly property color chipAccent: theme.channelColor(channelChip.modelData.accentKey)
                            checkable: true
                            checked: channelChip.modelData.active
                            hoverEnabled: true
                            implicitWidth: chipRow.implicitWidth + 18
                            implicitHeight: chipRow.implicitHeight + 8
                            Accessible.name: channelChip.modelData.label

                            onToggled: {
                                var k = channelChip.modelData.key
                                if (k === "temp") flightPage.showTempTrace = checked
                                else if (k === "load") flightPage.showLoadTrace = checked
                                else if (k === "psi") flightPage.showPsiTrace = checked
                                else if (k === "watts") flightPage.showWattsTrace = checked
                                else if (k === "battery") flightPage.showBatteryTrace = checked
                                else if (k === "gpu") flightPage.showGpuTrace = checked
                                else if (k === "disk") flightPage.showDiskTrace = checked
                                timelineCanvas.requestPaint()
                            }

                            background: Rectangle {
                                radius: theme.pillRadius
                                color: channelChip.checked
                                       ? theme.accentTint(channelChip.chipAccent, theme.isDark ? 0.20 : 0.12)
                                       : theme.subtleSurfaceColor
                                border.width: channelChip.checked ? 1.5 : 1
                                border.color: channelChip.activeFocus ? Kirigami.Theme.focusColor : channelChip.checked ? channelChip.chipAccent : theme.subtleBorderColor
                            }

                            contentItem: RowLayout {
                                id: chipRow
                                spacing: 6

                                Rectangle {
                                    Layout.preferredWidth: 8
                                    Layout.preferredHeight: 8
                                    radius: 4
                                    color: channelChip.chipAccent
                                    opacity: channelChip.checked ? 1.0 : 0.35
                                }

                                QQC2.Label {
                                    text: channelChip.modelData.label
                                    font.bold: channelChip.checked
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    color: channelChip.checked ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                                }
                            }
                        }
                    }
                }
            }
        }

        // Investigation Range & Comparative Analysis Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: rangeCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1
            border.color: theme.cardBorderColor

            ColumnLayout {
                id: rangeCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Kirigami.Heading {
                        level: 3
                        text: flightPage.controller.sessionMode ? "Frozen investigation session" : "Investigation range"
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        visible: flightPage.controller.sessionMode
                        radius: theme.pillRadius
                        color: theme.accentTint(theme.neutralColor, 0.16)
                        border.width: 1
                        border.color: theme.neutralColor
                        implicitWidth: sPillLbl.implicitWidth + 12
                        implicitHeight: sPillLbl.implicitHeight + 4
                        QQC2.Label {
                            id: sPillLbl
                            anchors.centerIn: parent
                            text: flightPage.controller.sessionPath ? "Saved session" : "In-memory investigation"
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            font.bold: true
                            color: theme.neutralColor
                        }
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    color: Kirigami.Theme.disabledTextColor
                    text: flightPage.controller.sessionMode
                        ? "Original observations are frozen. Live monitoring continues separately. " + (flightPage.controller.sessionPath || "")
                        : "Select a sample in the timeline, set the beginning and end, then freeze the range. Comparisons use up to 30 seconds on either side."
                }

                // Range Selection Actions Bar
                Flow {
                    visible: !flightPage.controller.sessionMode
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    PulseButton {
                        text: "Set beginning"
                        icon.name: "go-first"
                        enabled: flightPage.controller.selectedHistorySampleId !== 0
                        onClicked: { flightPage.rangeStartId = flightPage.controller.selectedHistorySampleId; flightPage.rangeStartTimeMs = Number(flightPage.inspectedDetail.timestampMs) }
                    }
                    PulseButton {
                        text: "Set end"
                        icon.name: "go-last"
                        enabled: flightPage.controller.selectedHistorySampleId !== 0
                        onClicked: { flightPage.rangeEndId = flightPage.controller.selectedHistorySampleId; flightPage.rangeEndTimeMs = Number(flightPage.inspectedDetail.timestampMs) }
                    }
                    PulseButton {
                        objectName: "freezeHistoryRangeButton"
                        text: "Freeze selected range"
                        icon.name: "media-playback-pause"
                        isPrimary: flightPage.rangeStartId > 0 && flightPage.rangeEndId >= flightPage.rangeStartId
                        enabled: flightPage.rangeStartId > 0 && flightPage.rangeEndId >= flightPage.rangeStartId
                        onClicked: flightPage.controller.selectHistoryRange(flightPage.rangeStartId, flightPage.rangeEndId)
                    }

                    Rectangle {
                        radius: theme.pillRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        implicitWidth: periodLbl.implicitWidth + 14
                        implicitHeight: periodLbl.implicitHeight + 8

                        QQC2.Label {
                            id: periodLbl
                            anchors.centerIn: parent
                            text: "Range: " + (flightPage.rangeStartTimeMs ? new Date(flightPage.rangeStartTimeMs).toLocaleTimeString() : "Start unset")
                                  + " → " + (flightPage.rangeEndTimeMs ? new Date(flightPage.rangeEndTimeMs).toLocaleTimeString() : "End unset")
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.textColor
                            font.bold: flightPage.rangeStartId > 0
                        }
                    }
                }

                // Before / During / After Comparison Tiles in Session Mode
                GridLayout {
                    visible: flightPage.controller.sessionMode
                    Layout.fillWidth: true
                    columns: flightPage.width < 720 ? 1 : 3
                    columnSpacing: Kirigami.Units.smallSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: flightPage.controller.sessionMode ? ["before", "during", "after"] : []
                        delegate: Rectangle {
                            id: compTile
                            required property string modelData
                            readonly property var windowData: flightPage.controller.investigationSummary[compTile.modelData]
                            Layout.fillWidth: true
                            implicitHeight: compTileCol.implicitHeight + 16
                            radius: theme.innerRadius
                            color: compTile.modelData === "during"
                                   ? theme.accentTint(theme.neutralColor, theme.isDark ? 0.16 : 0.09)
                                   : theme.cardNestedBg
                            border.width: 1
                            border.color: compTile.modelData === "during" ? theme.neutralColor : theme.subtleBorderColor

                            ColumnLayout {
                                id: compTileCol
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    QQC2.Label {
                                        text: compTile.modelData.toUpperCase()
                                        font.bold: true
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        color: compTile.modelData === "during" ? theme.neutralColor : Kirigami.Theme.textColor
                                        Layout.fillWidth: true
                                    }
                                    Rectangle {
                                        radius: theme.pillRadius
                                        color: theme.subtleSurfaceColor
                                        implicitWidth: countBadgeLbl.implicitWidth + 8
                                        implicitHeight: countBadgeLbl.implicitHeight + 4
                                        QQC2.Label {
                                            id: countBadgeLbl
                                            anchors.centerIn: parent
                                            text: (compTile.windowData ? compTile.windowData.sampleCount : 0) + " samples"
                                            font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                                            color: Kirigami.Theme.disabledTextColor
                                        }
                                    }
                                }

                                QQC2.Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    text: compTile.windowData
                                          ? "CPU avg: " + (compTile.windowData.cpuLoadAvailable ? Number(compTile.windowData.cpuLoadPercent).toFixed(1) + "%" : "—")
                                            + " · Responsiveness: " + (compTile.windowData.responsivenessAvailable ? Number(compTile.windowData.responsivenessScore).toFixed(1) + "/100" : "—")
                                            + "\nCoverage: " + (Number(compTile.windowData.coverageMs || 0) / 1000).toFixed(1) + " s · " + (compTile.windowData.gapCount || 0) + " gaps"
                                          : "No observations available"
                                }
                            }
                        }
                    }
                }
            }
        }

        // 2. Interactive Multi-Channel Canvas Timeline with Dedicated Y/X Axes
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 330
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: timelineCanvas.activeFocus ? 2 : 1
            border.color: timelineCanvas.activeFocus ? Kirigami.Theme.focusColor : theme.cardBorderColor

            // Left Y-Axis Labels Column (outside the plotted curve area so labels never overlap curves)
            ColumnLayout {
                id: yAxisColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.topMargin: 18
                anchors.bottomMargin: 32
                anchors.leftMargin: 10
                width: 72
                visible: flightPage.historyInfo.count >= 2

                QQC2.Label {
                    text: flightPage.yAxisLabelAt(1.0)
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                Item { Layout.fillHeight: true }
                QQC2.Label {
                    text: flightPage.yAxisLabelAt(0.75)
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                Item { Layout.fillHeight: true }
                QQC2.Label {
                    text: flightPage.yAxisLabelAt(0.5)
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                Item { Layout.fillHeight: true }
                QQC2.Label {
                    text: flightPage.yAxisLabelAt(0.25)
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                Item { Layout.fillHeight: true }
                QQC2.Label {
                    text: flightPage.yAxisLabelAt(0.0)
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
            }

            // Bottom X-Axis Timestamps Row
            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 88
                anchors.rightMargin: 16
                anchors.bottomMargin: 8
                visible: flightPage.historyInfo.count >= 2

                QQC2.Label {
                    text: flightPage.historyInfo.count > 0
                          ? (flightPage.historyInfo.firstLabel || "")
                          : ""
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                Item { Layout.fillWidth: true }
                QQC2.Label {
                    text: flightPage.historyInfo.count > 2
                          ? (flightPage.historyInfo.middleLabel || "")
                          : ""
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                Item { Layout.fillWidth: true }
                QQC2.Label {
                    text: flightPage.historyInfo.count > 0
                          ? (flightPage.historyInfo.lastLabel || "") + " (Latest)"
                          : ""
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
            }

            Canvas {
                id: timelineCanvas
                renderStrategy: Canvas.Threaded
                activeFocusOnTab: true
                objectName: "section_timeline"
                anchors.fill: parent
                anchors.topMargin: 18
                anchors.leftMargin: 88
                anchors.rightMargin: 16
                anchors.bottomMargin: 30
                focus: true
                property real hoverX: -1
                property var plotData: ({})
                function refreshPlot() {
                    if (visible && width > 0) plotData = JSON.parse(flightPage.controller.historyPlotJson(Math.ceil(width)))
                    requestPaint()
                }
                Component.onCompleted: refreshPlot()
                onWidthChanged: refreshPlot()
                onVisibleChanged: { if (visible) refreshPlot() }
                Accessible.role: Accessible.Graphic
                Accessible.name: "Hardware history timeline. Select a sample with left and right arrow keys."
                Accessible.description: "Inspected sample " + (flightPage.inspectedDetail.timeString || "unavailable")
                    + ". Responsiveness index " + (flightPage.inspectedDetail.psiAvailable
                      ? Number(flightPage.inspectedDetail.responsivenessScore).toFixed(1) + " out of 100; higher is better" : "unavailable")
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Home && flightPage.historyInfo.count) {
                        flightPage.controller.selectedHistorySampleId = flightPage.historyInfo.firstId
                        event.accepted = true
                    } else if (event.key === Qt.Key_End) {
                        flightPage.controller.selectedHistorySampleId = 0
                        event.accepted = true
                    }
                }
                Keys.onLeftPressed: flightPage.selectStep(-1)
                Keys.onRightPressed: flightPage.selectStep(1)

                Connections {
                    target: flightPage.controller
                    function onTelemetryUpdated() { timelineCanvas.refreshPlot() }
                    function onSelectedHistoryChanged() { timelineCanvas.requestPaint() }
                }

                function sampleMetricValue(sample, valueKey) {
                    if (valueKey === "diskTotalMibPerSec") {
                        return Number(sample.diskReadMibPerSec || 0) + Number(sample.diskWriteMibPerSec || 0)
                    }
                    return sample[valueKey]
                }

                function drawTrace(ctx, samples, firstTime, timeSpan, valueKey, availableKey, minValue, maxValue, color, fillAlpha) {
                    var pts = []
                    for (var i = 0; i < samples.length; ++i) {
                        var sample = samples[i]
                        var x = ((sample.timestampMs - firstTime) / timeSpan) * width
                        var normal = Math.min(1, Math.max(0, (sample.value - minValue) / Math.max(1, maxValue - minValue)))
                        pts.push({ x: x, y: height - normal * (height - 12) - 6, gap: sample.gapBefore === true })
                    }

                    // Bound raster path complexity for dense extrema. A single
                    // long zigzag makes the software stroker construct a costly
                    // self-intersecting outline even when points are pixel-bound.
                    var gradient = null
                    if (fillAlpha > 0) {
                        gradient = ctx.createLinearGradient(0, 0, 0, height)
                        gradient.addColorStop(0, Qt.rgba(color.r, color.g, color.b, fillAlpha).toString())
                        gradient.addColorStop(1, Qt.rgba(color.r, color.g, color.b, 0.0).toString())
                    }
                    ctx.strokeStyle = color.toString()
                    ctx.lineWidth = 2.2
                    ctx.lineJoin = "bevel"
                    ctx.lineCap = "butt"
                    var start = 0
                    while (start < pts.length) {
                        var end = start
                        while (end + 1 < pts.length && end - start < 4 && !pts[end + 1].gap) ++end
                        if (end > start) {
                            if (gradient) {
                                ctx.fillStyle = gradient
                                ctx.beginPath()
                                ctx.moveTo(pts[start].x, height)
                                for (var f = start; f <= end; ++f) ctx.lineTo(pts[f].x, pts[f].y)
                                ctx.lineTo(pts[end].x, height)
                                ctx.closePath()
                                ctx.fill()
                            }
                            ctx.beginPath()
                            ctx.moveTo(pts[start].x, pts[start].y)
                            for (var j = start + 1; j <= end; ++j) ctx.lineTo(pts[j].x, pts[j].y)
                            ctx.stroke()
                        } else {
                            ctx.fillStyle = color.toString()
                            ctx.fillRect(pts[start].x - 1, pts[start].y - 1, 2, 2)
                        }
                        start = end + 1 < pts.length && !pts[end + 1].gap ? end : end + 1
                    }
                }

                function drawMarkerAtSample(ctx, sample, firstTime, timeSpan, valueKey, availableKey, minValue, maxValue, color) {
                    if (!sample) return
                    var val = sampleMetricValue(sample, valueKey)
                    if (!sample[availableKey] || !isFinite(val)) return
                    var x = ((sample.timestampMs - firstTime) / timeSpan) * width
                    var normal = Math.min(1, Math.max(0, (val - minValue) / Math.max(1, maxValue - minValue)))
                    var y = height - normal * (height - 12) - 6
                    ctx.fillStyle = color.toString()
                    ctx.beginPath()
                    ctx.arc(x, y, 4.5, 0, Math.PI * 2)
                    ctx.fill()
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (flightPage.historyInfo.count < 2) return
                    var samples = timelineCanvas.plotData.markers || []
                    var firstTime = flightPage.historyInfo.firstTime
                    var lastTime = flightPage.historyInfo.lastTime
                    var timeSpan = Math.max(1, lastTime - firstTime)
                    var maxWatts = Math.max(65, flightPage.controller.historyMaxWatts)
                    var maxDiskMib = Math.max(50, flightPage.controller.historyMaxDiskMibPerSec)

                    // Horizontal grid lines (0%, 25%, 50%, 75%, 100%)
                    ctx.strokeStyle = theme.cardBorderColor.toString()
                    ctx.lineWidth = 1
                    for (var g = 0; g <= 4; ++g) {
                        var gy = 6 + ((height - 12) / 4) * g
                        ctx.beginPath()
                        ctx.moveTo(0, gy)
                        ctx.lineTo(width, gy)
                        ctx.stroke()
                    }

                    // Incident markers and visible breaks after pauses or sleep
                    ctx.globalAlpha = 0.25
                    for (var b = 0; b < samples.length; ++b) {
                        var bx = ((samples[b].timestampMs - firstTime) / timeSpan) * width
                        if (samples[b].gapBefore) {
                            ctx.fillStyle = Kirigami.Theme.disabledTextColor.toString()
                            ctx.fillRect(Math.max(0, bx - 3), 0, 6, height)
                        }
                        if (samples[b].throttled || samples[b].severity >= 2) {
                            ctx.fillStyle = samples[b].throttled
                                            ? Kirigami.Theme.negativeTextColor.toString()
                                            : Kirigami.Theme.neutralTextColor.toString()
                            ctx.fillRect(Math.max(0, bx - 1.5), 0, 3, height)
                        }
                    }
                    ctx.globalAlpha = 1.0

                    if (flightPage.showTempTrace)
                        drawTrace(ctx, timelineCanvas.plotData.cpuTemp || [], firstTime, timeSpan, "cpuTemp", "cpuTempAvailable", 25, 100, theme.tempAccent, 0.14)
                    if (flightPage.showLoadTrace)
                        drawTrace(ctx, timelineCanvas.plotData.cpuLoad || [], firstTime, timeSpan, "cpuLoad", "cpuLoadAvailable", 0, 100, theme.cpuAccent, 0.12)
                    if (flightPage.showPsiTrace)
                        drawTrace(ctx, timelineCanvas.plotData.responsivenessScore || [], firstTime, timeSpan, "responsivenessScore", "psiAvailable", 0, 100, theme.psiAccent, 0.08)
                    if (flightPage.showWattsTrace)
                        drawTrace(ctx, timelineCanvas.plotData.packageWatts || [], firstTime, timeSpan, "packageWatts", "packagePowerAvailable", 0, maxWatts, theme.powerAccent, 0.10)
                    if (flightPage.showBatteryTrace)
                        drawTrace(ctx, timelineCanvas.plotData.batteryWatts || [], firstTime, timeSpan, "batteryWatts", "batteryDischargeAvailable", 0, maxWatts, theme.batteryAccent, 0.06)
                    if (flightPage.showGpuTrace)
                        drawTrace(ctx, timelineCanvas.plotData.gpuBusy || [], firstTime, timeSpan, "gpuBusy", "gpuBusyAvailable", 0, 100, theme.gpuAccent, 0.10)
                    if (flightPage.showDiskTrace)
                        drawTrace(ctx, timelineCanvas.plotData.diskTotalMibPerSec || [], firstTime, timeSpan, "diskTotalMibPerSec", "diskThroughputAvailable", 0, maxDiskMib, theme.storageAccent, 0.10)

                    // Hover crosshair
                    if (timelineCanvas.hoverX >= 0 && timelineCanvas.hoverX <= width) {
                        ctx.globalAlpha = 0.5
                        ctx.strokeStyle = Kirigami.Theme.textColor.toString()
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(timelineCanvas.hoverX, 0)
                        ctx.lineTo(timelineCanvas.hoverX, height)
                        ctx.stroke()
                        ctx.globalAlpha = 1.0
                    }

                    // Active / Frozen crosshair & data point dots
                    var activeIdx = flightPage.hoveredSampleIndex >= 0
                                    ? flightPage.hoveredSampleIndex
                                    : flightPage.controller.selectedHistoryIndex
                    if (activeIdx >= 0 && activeIdx < flightPage.historyInfo.count) {
                        var actSample = flightPage.controller.historySampleAt(activeIdx)
                        var sx = ((actSample.timestampMs - firstTime) / timeSpan) * width
                        ctx.strokeStyle = Kirigami.Theme.highlightColor.toString()
                        ctx.lineWidth = 2
                        ctx.beginPath()
                        ctx.moveTo(sx, 0)
                        ctx.lineTo(sx, height)
                        ctx.stroke()

                        if (flightPage.showTempTrace)
                            drawMarkerAtSample(ctx, actSample, firstTime, timeSpan, "cpuTemp", "cpuTempAvailable", 25, 100, theme.tempAccent)
                        if (flightPage.showLoadTrace)
                            drawMarkerAtSample(ctx, actSample, firstTime, timeSpan, "cpuLoad", "cpuLoadAvailable", 0, 100, theme.cpuAccent)
                        if (flightPage.showPsiTrace)
                            drawMarkerAtSample(ctx, actSample, firstTime, timeSpan, "responsivenessScore", "psiAvailable", 0, 100, theme.psiAccent)
                        if (flightPage.showWattsTrace)
                            drawMarkerAtSample(ctx, actSample, firstTime, timeSpan, "packageWatts", "packagePowerAvailable", 0, maxWatts, theme.powerAccent)
                        if (flightPage.showBatteryTrace)
                            drawMarkerAtSample(ctx, actSample, firstTime, timeSpan, "batteryWatts", "batteryDischargeAvailable", 0, maxWatts, theme.batteryAccent)
                        if (flightPage.showGpuTrace)
                            drawMarkerAtSample(ctx, actSample, firstTime, timeSpan, "gpuBusy", "gpuBusyAvailable", 0, 100, theme.gpuAccent)
                        if (flightPage.showDiskTrace)
                            drawMarkerAtSample(ctx, actSample, firstTime, timeSpan, "diskTotalMibPerSec", "diskThroughputAvailable", 0, maxDiskMib, theme.storageAccent)
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.CrossCursor
                    function nearestIndexAtX(mouseX) {
                        return flightPage.controller.historyIndexAtRatio(mouseX / Math.max(1.0, width))
                    }
                    function selectAtX(mouseX) {
                        var nearest = nearestIndexAtX(mouseX)
                        if (nearest >= 0) flightPage.controller.selectedHistorySampleId = flightPage.controller.historySampleAt(nearest).sampleId
                    }
                    onPressed: (mouse) => { timelineCanvas.forceActiveFocus(); selectAtX(mouse.x) }
                    onPositionChanged: (mouse) => {
                        timelineCanvas.hoverX = mouse.x
                        flightPage.hoveredSampleIndex = nearestIndexAtX(mouse.x)
                        timelineCanvas.requestPaint()
                        if (pressed) selectAtX(mouse.x)
                    }
                    onExited: {
                        timelineCanvas.hoverX = -1
                        flightPage.hoveredSampleIndex = -1
                        timelineCanvas.requestPaint()
                    }
                }
            }

            QQC2.Label {
                anchors.centerIn: parent
                visible: flightPage.historyInfo.count === 0
                text: "No history has been recorded yet"
                color: Kirigami.Theme.disabledTextColor
            }
        }

        // Hover / Selection Status Strip
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: hoverStripLabel.implicitHeight + 16
            radius: theme.innerRadius
            color: flightPage.hoveredSampleIndex >= 0
                   ? theme.accentTint(Kirigami.Theme.highlightColor, theme.isDark ? 0.16 : 0.10)
                   : theme.subtleSurfaceColor
            border.width: 1
            border.color: flightPage.hoveredSampleIndex >= 0 ? Kirigami.Theme.highlightColor : theme.subtleBorderColor

            QQC2.Label {
                id: hoverStripLabel
                anchors.fill: parent
                anchors.margins: 8
                wrapMode: Text.WordWrap
                text: flightPage.hoveredSampleIndex >= 0
                      ? flightPage.hoveredSampleText()
                      : (flightPage.historyInfo.count
                         ? "Recorded window: " + new Date(flightPage.historyInfo.firstTime).toLocaleTimeString()
                           + " — " + new Date(flightPage.historyInfo.lastTime).toLocaleTimeString()
                           + "  ·  Click or drag on the timeline (or press Left/Right arrow keys) to inspect any sample."
                         : "No history yet. Measurements appear here after the next sample.")
                color: flightPage.hoveredSampleIndex >= 0 ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                font.bold: flightPage.hoveredSampleIndex >= 0
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            }
        }

        // 3. Recorded Incident Episodes Bar
        Rectangle {
            Layout.fillWidth: true
            visible: flightPage.controller.incidentHistory.length > 0
            implicitHeight: incCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1
            border.color: theme.cardBorderColor

            ColumnLayout {
                id: incCol
                objectName: "section_incidents"
                activeFocusOnTab: true
                Accessible.name: "incidents section"
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    text: "Recorded incidents (open original evidence and comparison):"
                    font.bold: true
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: flightPage.controller.incidentHistory
                        delegate: PulseButton {
                            id: incidentChip
                            required property var modelData
                            icon.name: incidentChip.modelData.throttled || incidentChip.modelData.severity >= 2 ? "dialog-warning" : "dialog-information"
                            accentColor: incidentChip.modelData.throttled || incidentChip.modelData.severity >= 2 ? theme.negativeColor : theme.neutralColor
                            text: incidentChip.modelData.timeString
                                  + (incidentChip.modelData.durationSeconds > 1 ? " (" + incidentChip.modelData.durationSeconds + "s)" : "")
                                  + " · " + incidentChip.modelData.headline
                            onClicked: flightPage.controller.selectIncident(incidentChip.modelData.incidentId)
                        }
                    }
                }
            }
        }

        // 4. Snapshot Inspector HUD Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: inspectorCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1.5
            border.color: flightPage.controller.selectedHistorySampleId === 0
                          ? theme.cardBorderColor
                          : Kirigami.Theme.highlightColor

            ColumnLayout {
                id: inspectorCol
                objectName: "section_inspector"
                activeFocusOnTab: true
                Accessible.name: "inspector section"
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Rectangle {
                        radius: theme.pillRadius
                        color: flightPage.controller.selectedHistorySampleId === 0
                               ? theme.accentTint(Kirigami.Theme.positiveTextColor, 0.16)
                               : theme.accentTint(Kirigami.Theme.highlightColor, 0.18)
                        border.width: 1
                        border.color: flightPage.controller.selectedHistorySampleId === 0
                                      ? Kirigami.Theme.positiveTextColor
                                      : Kirigami.Theme.highlightColor
                        implicitWidth: modePillLbl.implicitWidth + 14
                        implicitHeight: modePillLbl.implicitHeight + 6

                        QQC2.Label {
                            id: modePillLbl
                            anchors.centerIn: parent
                            text: flightPage.controller.selectedHistorySampleId === 0
                                  ? "LIVE STREAM"
                                  : (flightPage.controller.selectedHistoryExpired ? "FROZEN (EXPIRED)" : "FROZEN SAMPLE")
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: flightPage.controller.selectedHistorySampleId === 0
                                   ? Kirigami.Theme.positiveTextColor
                                   : Kirigami.Theme.highlightColor
                        }
                    }

                    Kirigami.Heading {
                        level: 3
                        text: "Inspected moment: " + (flightPage.inspectedDetail.timeString || "--:--:--")
                        color: Kirigami.Theme.textColor
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    PulseButton {
                        text: "Open System Monitor"
                        icon.name: "utilities-system-monitor"
                        onClicked: flightPage.controller.openSystemMonitor()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: statusAtTimeLbl.implicitHeight + 14
                    radius: theme.innerRadius
                    color: theme.subtleSurfaceColor
                    border.width: 1
                    border.color: theme.subtleBorderColor

                    QQC2.Label {
                        id: statusAtTimeLbl
                        anchors.fill: parent
                        anchors.margins: 7
                        text: "Status at this time: " + (flightPage.inspectedDetail.verdictHeadline || "No recorded sample")
                        color: Kirigami.Theme.highlightColor
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                }

                // 6-Card Inspector Telemetry Grid
                GridLayout {
                    Layout.fillWidth: true
                    columns: flightPage.width < 640 ? 1 : (flightPage.width < 920 ? 2 : 3)
                    rowSpacing: Kirigami.Units.smallSpacing
                    columnSpacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: [
                            {
                                label: "CPU load & frequency",
                                accentKey: "cpu",
                                value: flightPage.inspectedDetail.cpuLoadAvailable
                                    ? Number(flightPage.inspectedDetail.cpuLoad).toFixed(1) + "%"
                                      + (flightPage.inspectedDetail.cpuFrequencyAvailable
                                         ? " · " + flightPage.inspectedDetail.cpuFreqMhz + " MHz" : "")
                                    : "Unavailable",
                                sub: "New throttle events: " + (!flightPage.inspectedDetail.throttleCountersAvailable
                                    ? "Unavailable" : (flightPage.inspectedDetail.throttled ? "Observed" : "None observed"))
                            },
                            {
                                label: "CPU & NVMe thermals",
                                accentKey: "temp",
                                value: "CPU " + (flightPage.inspectedDetail.cpuTempAvailable
                                    ? Number(flightPage.inspectedDetail.cpuTemp).toFixed(1) + " °C" : "Unavailable"),
                                sub: "NVMe: " + (flightPage.inspectedDetail.nvmeTempAvailable
                                    ? Number(flightPage.inspectedDetail.nvmeTemp).toFixed(1) + " °C" : "Unavailable")
                            },
                            {
                                label: "Package power",
                                accentKey: "power",
                                value: flightPage.inspectedDetail.packagePowerAvailable
                                    ? Number(flightPage.inspectedDetail.packageWatts).toFixed(1) + " W"
                                    : "Unavailable",
                                sub: flightPage.inspectedDetail.packagePowerAvailable
                                    ? (flightPage.inspectedDetail.packagePowerEstimated ? "Estimated power" : "Measured hardware power")
                                    : "No power channel"
                            },
                            {
                                label: "Battery discharge",
                                accentKey: "battery",
                                value: flightPage.inspectedDetail.batteryDischargeAvailable
                                    ? Number(flightPage.inspectedDetail.batteryWatts).toFixed(1) + " W" : "Unavailable",
                                sub: "Battery discharge channel; separate from CPU package power"
                            },
                            {
                                label: "Graphics (GPU)",
                                accentKey: "gpu",
                                value: (flightPage.inspectedDetail.gpuBusyAvailable
                                    ? Number(flightPage.inspectedDetail.gpuBusy).toFixed(0) + "% load" : "Load N/A")
                                    + " · " + (flightPage.inspectedDetail.gpuTempAvailable
                                    ? Number(flightPage.inspectedDetail.gpuTemp).toFixed(0) + " °C" : "Temp N/A"),
                                sub: "GPU telemetry at sample"
                            },
                            {
                                label: "Pressure & Disk I/O",
                                accentKey: "psi",
                                value: "Responsiveness index: " + (flightPage.inspectedDetail.psiAvailable
                                    ? Number(flightPage.inspectedDetail.responsivenessScore).toFixed(1) + "/100" : "Unavailable"),
                                sub: flightPage.inspectedDetail.diskThroughputAvailable
                                    ? "Disk R " + Number(flightPage.inspectedDetail.diskReadMibPerSec).toFixed(1)
                                      + " · W " + Number(flightPage.inspectedDetail.diskWriteMibPerSec).toFixed(1) + " MiB/s"
                                    : "Disk I/O throughput: Unavailable"
                            },
                            {
                                label: "Concurrent process",
                                accentKey: "storage",
                                value: (flightPage.inspectedDetail.culpritName || "Unavailable")
                                    + (flightPage.inspectedDetail.culpritName
                                       ? " (PID " + flightPage.inspectedDetail.culpritPid + ")" : ""),
                                sub: flightPage.inspectedDetail.culpritName
                                    ? Number(flightPage.inspectedDetail.culpritCpuPercent).toFixed(1)
                                      + "% CPU · " + Math.round(Number(flightPage.inspectedDetail.culpritRssMb)) + " MiB RSS"
                                    : "Observed process use: Unavailable"
                            }
                        ]

                        delegate: Rectangle {
                            id: snapTile
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: snapCol.implicitHeight + 20
                            radius: theme.innerRadius
                            color: theme.subtleSurfaceColor
                            border.width: 1
                            border.color: theme.subtleBorderColor

                            ColumnLayout {
                                id: snapCol
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 3

                                QQC2.Label {
                                    text: snapTile.modelData.label
                                    color: theme.channelColor(snapTile.modelData.accentKey)
                                    font.bold: true
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }

                                QQC2.Label {
                                    text: snapTile.modelData.value
                                    color: Kirigami.Theme.textColor
                                    font.bold: true
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }

                                QQC2.Label {
                                    text: snapTile.modelData.sub
                                    color: Kirigami.Theme.disabledTextColor
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }

                QQC2.GroupBox {
                    Layout.fillWidth: true
                    title: "Recorded process observations"
                    ColumnLayout {
                        width: parent.width
                        QQC2.Label {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            text: "Observed every 2 seconds: the union of the top 8 CPU, memory and disk-wait processes. This is a bounded sample, not a complete historical process list. Process identities are excluded from saved files unless enabled in Settings."
                        }
                        QQC2.Label {
                            visible: !(flightPage.inspectedDetail.processObservations || []).length
                            text: "No process identities recorded at this moment"
                        }
                        Repeater {
                            model: flightPage.inspectedDetail.processObservations || []
                            delegate: QQC2.Label {
                                required property var modelData
                                Layout.fillWidth: true; wrapMode: Text.WordWrap
                                text: (modelData.name || "Identity omitted") + (modelData.pid ? " · PID " + modelData.pid : "")
                                      + " · " + (modelData.cpuAvailable === false ? "CPU warming up" : Number(modelData.cpuPercent || 0).toFixed(1) + "% CPU")
                                      + " · " + Number(modelData.rssMegabytes || 0).toFixed(1) + " MiB"
                                      + " · Observed " + new Date(Number(modelData.observedAtMs)).toLocaleTimeString()
                            }
                        }
                    }
                }

                QQC2.GroupBox {
                    Layout.fillWidth: true
                    title: "Recorded data quality"
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 4
                        Repeater {
                            model: ["cpuLoad", "cpuTemperature", "gpuLoad", "pressureIndex", "diskThroughput", "packagePower"]
                            delegate: QQC2.Label {
                                required property string modelData
                                readonly property var quality: flightPage.inspectedDetail.metricMetadata
                                                               ? flightPage.inspectedDetail.metricMetadata[modelData] : null
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                color: Kirigami.Theme.disabledTextColor
                                text: MeasurementLabels.metricLabel(modelData) + ": " + (quality ? MeasurementLabels.qualityLabel(quality.state) + " · " + quality.source
                                      + " · " + new Date(quality.sampledAtMs).toLocaleTimeString() : "Unavailable")
                            }
                        }
                    }
                }
            }
        }
    }
}
}
