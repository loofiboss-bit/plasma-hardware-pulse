pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Qt.labs.settings

Kirigami.ScrollablePage {
    id: flightPage
    objectName: "historyPage"
    title: "History"

    required property var controller

    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: flightPage.Kirigami.Theme.backgroundColor
        disabledColor: flightPage.Kirigami.Theme.disabledTextColor
        negativeColor: flightPage.Kirigami.Theme.negativeTextColor
        neutralColor: flightPage.Kirigami.Theme.neutralTextColor
        positiveColor: flightPage.Kirigami.Theme.positiveTextColor
    }

    Settings {
        id: historySettings
        category: "LoofiPulseHistory"
        property bool showTemp: true
        property bool showLoad: true
        property bool showPressure: true
        property bool showPower: false
        property bool showGpu: false
        property bool showDisk: false
    }

    property bool showTempTrace: historySettings.showTemp
    property bool showLoadTrace: historySettings.showLoad
    property bool showPsiTrace: historySettings.showPressure
    property bool showWattsTrace: historySettings.showPower
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
        var samples = controller.historySamples
        if (!samples || samples.length === 0) return
        var index = controller.selectedHistoryIndex
        index = index < 0 ? samples.length - 1 : Math.max(0, Math.min(samples.length - 1, index + delta))
        controller.selectedHistorySampleId = samples[index].sampleId
    }

    function yAxisLabelAt(fraction) {
        var parts = []
        if (flightPage.showLoadTrace || flightPage.showPsiTrace || flightPage.showGpuTrace) {
            parts.push(Math.round(fraction * 100) + "%")
        }
        if (flightPage.showTempTrace && (fraction === 0 || fraction === 0.5 || fraction === 1.0)) {
            parts.push(Math.round(25 + fraction * 75) + "°")
        }
        if (flightPage.showWattsTrace && (fraction === 0.5 || fraction === 1.0)) {
            parts.push(Math.round(fraction * Math.max(65, flightPage.controller.historyMaxWatts)) + "W")
        }
        if (flightPage.showDiskTrace && (fraction === 0.5 || fraction === 1.0)) {
            parts.push(Math.round(fraction * Math.max(50, flightPage.controller.historyMaxDiskMibPerSec)) + "M")
        }
        return parts.length > 0 ? parts.join(" · ") : (Math.round(fraction * 100) + "%")
    }

    function hoveredSampleText() {
        var samples = flightPage.controller.historySamples
        if (!samples || flightPage.hoveredSampleIndex < 0 || flightPage.hoveredSampleIndex >= samples.length) {
            return ""
        }
        var s = samples[flightPage.hoveredSampleIndex]
        var parts = ["Hover @ " + (s.timeString || "--:--:--")]
        if (s.cpuTempAvailable) parts.push("CPU " + Number(s.cpuTemp).toFixed(1) + " °C")
        if (s.cpuLoadAvailable) parts.push("Load " + Number(s.cpuLoad).toFixed(0) + "%")
        if (s.psiAvailable) parts.push("Pressure idx " + Number(s.responsivenessScore).toFixed(1) + "%")
        if (s.systemPowerAvailable) parts.push(Number(s.systemWatts).toFixed(1) + " W")
        if (s.diskThroughputAvailable) parts.push("Disk " + Number(s.diskReadMibPerSec + s.diskWriteMibPerSec).toFixed(1) + " MiB/s")
        parts.push("Click to freeze moment")
        return parts.join("  ·  ")
    }

    ColumnLayout {
        Layout.fillWidth: true
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

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Button {
                        icon.name: "dialog-warning"
                        text: "Peak Thermal Spike"
                        onClicked: flightPage.controller.jumpToPeakThermalSpike()
                    }

                    QQC2.Button {
                        icon.name: "chronometer"
                        text: "Worst PSI Stall"
                        onClicked: flightPage.controller.jumpToWorstStallSpike()
                    }

                    QQC2.Button {
                        icon.name: "media-playback-start"
                        text: "Follow Live"
                        enabled: flightPage.controller.selectedHistorySampleId !== 0
                        onClicked: flightPage.controller.selectedHistorySampleId = 0
                    }

                    QQC2.Button {
                        icon.name: flightPage.controller.recordingPaused ? "media-playback-start" : "media-playback-pause"
                        text: flightPage.controller.recordingPaused ? "Resume Recording" : "Pause Recording"
                        checkable: true
                        checked: flightPage.controller.recordingPaused
                        onClicked: flightPage.controller.recordingPaused = !flightPage.controller.recordingPaused
                    }

                    QQC2.Button {
                        objectName: "clearHistoryButton"
                        icon.name: "edit-clear-history"
                        text: "Clear History"
                        enabled: flightPage.controller.historySamples.length > 0
                        onClicked: flightPage.controller.clearHistory()
                    }

                    QQC2.Button {
                        icon.name: "edit-copy"
                        text: "Copy Report (MD)"
                        onClicked: flightPage.controller.copyIncidentReportToClipboard()
                    }

                    QQC2.Button {
                        icon.name: "document-save"
                        text: "Save Report + JSON"
                        onClicked: flightPage.controller.exportIncidentReportToFile()
                    }

                    QQC2.Button {
                        visible: flightPage.controller.lastActionPath.length > 0
                        icon.name: "folder-open"
                        text: "Open reports folder"
                        onClicked: flightPage.controller.openReportsFolder()
                    }

                    Repeater {
                        model: [
                            { seconds: 300, label: "5m" },
                            { seconds: 900, label: "15m" },
                            { seconds: 1800, label: "30m" }
                        ]
                        delegate: QQC2.Button {
                            id: retentionBtn
                            required property var modelData
                            text: retentionBtn.modelData.label + " window"
                            checkable: true
                            checked: flightPage.controller.historyRetentionSeconds === retentionBtn.modelData.seconds
                            onClicked: flightPage.controller.historyRetentionSeconds = retentionBtn.modelData.seconds
                        }
                    }

                    Rectangle {
                        radius: theme.pillRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        implicitWidth: sampleCountLbl.implicitWidth + 16
                        implicitHeight: sampleCountLbl.implicitHeight + 10

                        QQC2.Label {
                            id: sampleCountLbl
                            anchors.centerIn: parent
                            text: flightPage.controller.historySamples.length + " samples · "
                                  + Math.round(flightPage.controller.historyRetentionSeconds / 60) + " min window"
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
                                label: "Pressure index (0–100%)",
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
                            implicitWidth: chipRow.implicitWidth + 20
                            implicitHeight: chipRow.implicitHeight + 10
                            Accessible.name: channelChip.modelData.label

                            onToggled: {
                                var k = channelChip.modelData.key
                                if (k === "temp") flightPage.showTempTrace = checked
                                else if (k === "load") flightPage.showLoadTrace = checked
                                else if (k === "psi") flightPage.showPsiTrace = checked
                                else if (k === "watts") flightPage.showWattsTrace = checked
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
                                border.color: channelChip.checked ? channelChip.chipAccent : theme.subtleBorderColor
                            }

                            contentItem: RowLayout {
                                id: chipRow
                                spacing: 6

                                Rectangle {
                                    Layout.preferredWidth: 10
                                    Layout.preferredHeight: 10
                                    radius: 5
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
                visible: flightPage.controller.historySamples.length >= 2

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
                visible: flightPage.controller.historySamples.length >= 2

                QQC2.Label {
                    text: flightPage.controller.historySamples.length > 0
                          ? (flightPage.controller.historySamples[0].timeString || "")
                          : ""
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                Item { Layout.fillWidth: true }
                QQC2.Label {
                    text: flightPage.controller.historySamples.length > 2
                          ? (flightPage.controller.historySamples[Math.floor(flightPage.controller.historySamples.length / 2)].timeString || "")
                          : ""
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                Item { Layout.fillWidth: true }
                QQC2.Label {
                    text: flightPage.controller.historySamples.length > 0
                          ? (flightPage.controller.historySamples[flightPage.controller.historySamples.length - 1].timeString || "") + " (Latest)"
                          : ""
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
            }

            Canvas {
                id: timelineCanvas
                objectName: "historyTimeline"
                anchors.fill: parent
                anchors.topMargin: 18
                anchors.leftMargin: 88
                anchors.rightMargin: 16
                anchors.bottomMargin: 30
                focus: true
                property real hoverX: -1
                Accessible.name: "Hardware history timeline. Select a sample with left and right arrow keys."
                Keys.onLeftPressed: flightPage.selectStep(-1)
                Keys.onRightPressed: flightPage.selectStep(1)

                Connections {
                    target: flightPage.controller
                    function onTelemetryUpdated() { timelineCanvas.requestPaint() }
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
                        var val = sampleMetricValue(sample, valueKey)
                        if (!sample[availableKey] || !isFinite(val)) {
                            pts.push(null)
                            continue
                        }
                        var x = ((sample.timestampMs - firstTime) / timeSpan) * width
                        var normal = Math.min(1, Math.max(0, (val - minValue) / Math.max(1, maxValue - minValue)))
                        var y = height - normal * (height - 12) - 6
                        pts.push({ x: x, y: y, gap: sample.gapBefore === true })
                    }

                    // Draw translucent gradient fill under continuous segments
                    if (fillAlpha > 0) {
                        var grad = ctx.createLinearGradient(0, 0, 0, height)
                        grad.addColorStop(0, Qt.rgba(color.r, color.g, color.b, fillAlpha).toString())
                        grad.addColorStop(1, Qt.rgba(color.r, color.g, color.b, 0.0).toString())
                        ctx.fillStyle = grad
                        var segStart = -1
                        for (var k = 0; k < pts.length; ++k) {
                            if (pts[k] !== null && !pts[k].gap) {
                                if (segStart < 0) segStart = k
                            } else {
                                if (segStart >= 0 && (k - 1) > segStart) {
                                    ctx.beginPath()
                                    ctx.moveTo(pts[segStart].x, height)
                                    for (var m = segStart; m < k; ++m) ctx.lineTo(pts[m].x, pts[m].y)
                                    ctx.lineTo(pts[k - 1].x, height)
                                    ctx.closePath()
                                    ctx.fill()
                                }
                                segStart = (pts[k] !== null) ? k : -1
                            }
                        }
                        if (segStart >= 0 && (pts.length - 1) > segStart) {
                            ctx.beginPath()
                            ctx.moveTo(pts[segStart].x, height)
                            for (var n = segStart; n < pts.length; ++n) ctx.lineTo(pts[n].x, pts[n].y)
                            ctx.lineTo(pts[pts.length - 1].x, height)
                            ctx.closePath()
                            ctx.fill()
                        }
                    }

                    // Draw crisp curve stroke
                    ctx.strokeStyle = color.toString()
                    ctx.lineWidth = 2.2
                    ctx.beginPath()
                    var drawing = false
                    for (var j = 0; j < pts.length; ++j) {
                        var pt = pts[j]
                        if (pt === null) {
                            drawing = false
                            continue
                        }
                        if (!drawing || pt.gap) ctx.moveTo(pt.x, pt.y)
                        else ctx.lineTo(pt.x, pt.y)
                        drawing = true
                    }
                    ctx.stroke()
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
                    var samples = flightPage.controller.historySamples
                    if (!samples || samples.length < 2) return
                    var firstTime = samples[0].timestampMs
                    var lastTime = samples[samples.length - 1].timestampMs
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
                        drawTrace(ctx, samples, firstTime, timeSpan, "cpuTemp", "cpuTempAvailable", 25, 100, theme.tempAccent, 0.14)
                    if (flightPage.showLoadTrace)
                        drawTrace(ctx, samples, firstTime, timeSpan, "cpuLoad", "cpuLoadAvailable", 0, 100, theme.cpuAccent, 0.12)
                    if (flightPage.showPsiTrace)
                        drawTrace(ctx, samples, firstTime, timeSpan, "responsivenessScore", "psiAvailable", 0, 100, theme.psiAccent, 0.08)
                    if (flightPage.showWattsTrace)
                        drawTrace(ctx, samples, firstTime, timeSpan, "systemWatts", "systemPowerAvailable", 0, maxWatts, theme.powerAccent, 0.10)
                    if (flightPage.showGpuTrace)
                        drawTrace(ctx, samples, firstTime, timeSpan, "gpuBusy", "gpuBusyAvailable", 0, 100, theme.gpuAccent, 0.10)
                    if (flightPage.showDiskTrace)
                        drawTrace(ctx, samples, firstTime, timeSpan, "diskTotalMibPerSec", "diskThroughputAvailable", 0, maxDiskMib, theme.storageAccent, 0.10)

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
                    if (activeIdx >= 0 && activeIdx < samples.length) {
                        var actSample = samples[activeIdx]
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
                            drawMarkerAtSample(ctx, actSample, firstTime, timeSpan, "systemWatts", "systemPowerAvailable", 0, maxWatts, theme.powerAccent)
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
                        var samples = flightPage.controller.historySamples
                        if (!samples || samples.length === 0) return -1
                        var ratio = Math.min(1.0, Math.max(0.0, mouseX / Math.max(1.0, width)))
                        var targetTime = samples[0].timestampMs
                                + ratio * (samples[samples.length - 1].timestampMs - samples[0].timestampMs)
                        var nearest = 0
                        for (var i = 1; i < samples.length; ++i) {
                            if (Math.abs(samples[i].timestampMs - targetTime)
                                    < Math.abs(samples[nearest].timestampMs - targetTime)) nearest = i
                        }
                        return nearest
                    }
                    function selectAtX(mouseX) {
                        var samples = flightPage.controller.historySamples
                        var nearest = nearestIndexAtX(mouseX)
                        if (nearest >= 0 && nearest < samples.length) {
                            flightPage.controller.selectedHistorySampleId = samples[nearest].sampleId
                        }
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
                visible: flightPage.controller.historySamples.length === 0
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
                      : (flightPage.controller.historySamples.length
                         ? "Recorded window: " + new Date(flightPage.controller.historySamples[0].timestampMs).toLocaleTimeString()
                           + " — " + new Date(flightPage.controller.historySamples[flightPage.controller.historySamples.length - 1].timestampMs).toLocaleTimeString()
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
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    text: "Recorded incidents (click to jump to peak moment):"
                    font.bold: true
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: flightPage.controller.incidentHistory
                        delegate: QQC2.Button {
                            id: incidentChip
                            required property var modelData
                            icon.name: incidentChip.modelData.throttled || incidentChip.modelData.severity >= 2 ? "dialog-warning" : "dialog-information"
                            text: incidentChip.modelData.timeString
                                  + (incidentChip.modelData.durationSeconds > 1 ? " (" + incidentChip.modelData.durationSeconds + "s)" : "")
                                  + " · " + incidentChip.modelData.headline
                            onClicked: flightPage.controller.selectedHistorySampleId = incidentChip.modelData.sampleId
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
                        text: "Inspected moment: " + (flightPage.controller.selectedHistoryDetail.timeString || "--:--:--")
                        color: Kirigami.Theme.textColor
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    QQC2.Button {
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
                        text: "Status at this time: " + (flightPage.controller.selectedHistoryDetail.verdictHeadline || "No recorded sample")
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
                                value: flightPage.controller.selectedHistoryDetail.cpuLoadAvailable
                                    ? Number(flightPage.controller.selectedHistoryDetail.cpuLoad).toFixed(1) + "%"
                                      + (flightPage.controller.selectedHistoryDetail.cpuFrequencyAvailable
                                         ? " · " + flightPage.controller.selectedHistoryDetail.cpuFreqMhz + " MHz" : "")
                                    : "Unavailable",
                                sub: "New throttle events: " + (!flightPage.controller.selectedHistoryDetail.throttleCountersAvailable
                                    ? "Unavailable" : (flightPage.controller.selectedHistoryDetail.throttled ? "Observed" : "None observed"))
                            },
                            {
                                label: "CPU & NVMe thermals",
                                accentKey: "temp",
                                value: "CPU " + (flightPage.controller.selectedHistoryDetail.cpuTempAvailable
                                    ? Number(flightPage.controller.selectedHistoryDetail.cpuTemp).toFixed(1) + " °C" : "Unavailable"),
                                sub: "NVMe: " + (flightPage.controller.selectedHistoryDetail.nvmeTempAvailable
                                    ? Number(flightPage.controller.selectedHistoryDetail.nvmeTemp).toFixed(1) + " °C" : "Unavailable")
                            },
                            {
                                label: "System power draw",
                                accentKey: "power",
                                value: flightPage.controller.selectedHistoryDetail.systemPowerAvailable
                                    ? Number(flightPage.controller.selectedHistoryDetail.systemWatts).toFixed(1) + " W"
                                    : "Unavailable",
                                sub: flightPage.controller.selectedHistoryDetail.systemPowerAvailable
                                    ? (flightPage.controller.selectedHistoryDetail.systemPowerEstimated ? "Estimated power" : "Measured hardware power")
                                    : "No power channel"
                            },
                            {
                                label: "Graphics (GPU)",
                                accentKey: "gpu",
                                value: (flightPage.controller.selectedHistoryDetail.gpuBusyAvailable
                                    ? Number(flightPage.controller.selectedHistoryDetail.gpuBusy).toFixed(0) + "% load" : "Load N/A")
                                    + " · " + (flightPage.controller.selectedHistoryDetail.gpuTempAvailable
                                    ? Number(flightPage.controller.selectedHistoryDetail.gpuTemp).toFixed(0) + " °C" : "Temp N/A"),
                                sub: "GPU telemetry at sample"
                            },
                            {
                                label: "Pressure & Disk I/O",
                                accentKey: "psi",
                                value: "Pressure idx: " + (flightPage.controller.selectedHistoryDetail.psiAvailable
                                    ? Number(flightPage.controller.selectedHistoryDetail.responsivenessScore).toFixed(1) + "%" : "Unavailable"),
                                sub: flightPage.controller.selectedHistoryDetail.diskThroughputAvailable
                                    ? "Disk R " + Number(flightPage.controller.selectedHistoryDetail.diskReadMibPerSec).toFixed(1)
                                      + " · W " + Number(flightPage.controller.selectedHistoryDetail.diskWriteMibPerSec).toFixed(1) + " MiB/s"
                                    : "Disk I/O throughput: Unavailable"
                            },
                            {
                                label: "Concurrent process",
                                accentKey: "storage",
                                value: (flightPage.controller.selectedHistoryDetail.culpritName || "Unavailable")
                                    + (flightPage.controller.selectedHistoryDetail.culpritName
                                       ? " (PID " + flightPage.controller.selectedHistoryDetail.culpritPid + ")" : ""),
                                sub: flightPage.controller.selectedHistoryDetail.culpritName
                                    ? Number(flightPage.controller.selectedHistoryDetail.culpritCpuPercent).toFixed(1)
                                      + "% CPU · " + Math.round(Number(flightPage.controller.selectedHistoryDetail.culpritRssMb)) + " MiB RSS"
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
                    title: "Recorded data quality"
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 4
                        Repeater {
                            model: ["cpuLoad", "cpuTemperature", "gpuLoad", "pressureIndex", "diskThroughput", "packagePower"]
                            delegate: QQC2.Label {
                                required property string modelData
                                readonly property var quality: flightPage.controller.selectedHistoryDetail.metricMetadata
                                                               ? flightPage.controller.selectedHistoryDetail.metricMetadata[modelData] : null
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                color: Kirigami.Theme.disabledTextColor
                                text: modelData + ": " + (quality ? quality.state + " · " + quality.source
                                      + " · " + new Date(quality.sampledAtMs).toLocaleTimeString() : "Unavailable")
                            }
                        }
                    }
                }
            }
        }
    }
}
