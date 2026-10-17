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
    Settings {
        id: historySettings
        category: "LoofiPulseHistory"
        property bool showTemp: true
        property bool showLoad: true
        property bool showPressure: true
        property bool showPower: false
    }
    property bool showTempTrace: historySettings.showTemp
    property bool showLoadTrace: historySettings.showLoad
    property bool showPsiTrace: historySettings.showPressure
    property bool showWattsTrace: historySettings.showPower
    onShowTempTraceChanged: historySettings.showTemp = showTempTrace
    onShowLoadTraceChanged: historySettings.showLoad = showLoadTrace
    onShowPsiTraceChanged: historySettings.showPressure = showPsiTrace
    onShowWattsTraceChanged: historySettings.showPower = showWattsTrace
    function selectStep(delta) {
        var samples = controller.historySamples
        if (!samples || samples.length === 0) return
        var index = controller.selectedHistoryIndex
        index = index < 0 ? samples.length - 1 : Math.max(0, Math.min(samples.length - 1, index + delta))
        controller.selectedHistorySampleId = samples[index].sampleId
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

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

            QQC2.Label {
                text: flightPage.controller.historySamples.length + " samples · 5 min"
                color: Kirigami.Theme.disabledTextColor
                font.bold: true
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.CheckBox {
                text: "CPU temperature (25–100 °C)"
                checked: flightPage.showTempTrace
                onToggled: { flightPage.showTempTrace = checked; timelineCanvas.requestPaint() }
            }
            QQC2.CheckBox {
                text: "CPU load (0–100%)"
                checked: flightPage.showLoadTrace
                onToggled: { flightPage.showLoadTrace = checked; timelineCanvas.requestPaint() }
            }
            QQC2.CheckBox {
                text: "Calculated pressure index (0–100%)"
                checked: flightPage.showPsiTrace
                onToggled: { flightPage.showPsiTrace = checked; timelineCanvas.requestPaint() }
            }
            QQC2.CheckBox {
                text: "Power (0–65 W)"
                checked: flightPage.showWattsTrace
                onToggled: { flightPage.showWattsTrace = checked; timelineCanvas.requestPaint() }
            }
        }

        // Interactive Multi-Channel Canvas Timeline
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 295
            radius: 12
            color: Kirigami.Theme.alternateBackgroundColor
            border.width: timelineCanvas.activeFocus ? 2 : 1
            border.color: timelineCanvas.activeFocus ? Kirigami.Theme.focusColor : Kirigami.Theme.disabledTextColor

            Canvas {
                id: timelineCanvas
                objectName: "historyTimeline"
                anchors.fill: parent
                focus: true
                Accessible.name: "Five minute hardware history. Select a sample with left and right arrow keys."
                Keys.onLeftPressed: flightPage.selectStep(-1)
                Keys.onRightPressed: flightPage.selectStep(1)
                anchors.topMargin: 16
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.bottomMargin: 16

                Connections {
                    target: flightPage.controller
                    function onTelemetryUpdated() { timelineCanvas.requestPaint() }
                    function onSelectedHistoryChanged() { timelineCanvas.requestPaint() }
                }

                function drawTrace(ctx, samples, firstTime, timeSpan, valueKey, availableKey, minValue, maxValue, color) {
                    ctx.strokeStyle = color.toString()
                    ctx.lineWidth = 2
                    ctx.beginPath()
                    var drawing = false
                    for (var i = 0; i < samples.length; ++i) {
                        var sample = samples[i]
                        if (!sample[availableKey] || !isFinite(sample[valueKey])) {
                            drawing = false
                            continue
                        }
                        var x = ((sample.timestampMs - firstTime) / timeSpan) * width
                        var normal = Math.min(1, Math.max(0, (sample[valueKey] - minValue) / (maxValue - minValue)))
                        var y = height - normal * (height - 12) - 6
                        if (!drawing || sample.gapBefore) ctx.moveTo(x, y)
                        else ctx.lineTo(x, y)
                        drawing = true
                    }
                    ctx.stroke()
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    var samples = flightPage.controller.historySamples
                    if (!samples || samples.length < 2) return
                    var firstTime = samples[0].timestampMs
                    var lastTime = samples[samples.length - 1].timestampMs
                    var timeSpan = Math.max(1, lastTime - firstTime)

                    // Incident markers and visible breaks after pauses or sleep.
                    ctx.globalAlpha = 0.28
                    for (var b = 0; b < samples.length; ++b) {
                        var bx = ((samples[b].timestampMs - firstTime) / timeSpan) * width
                        if (samples[b].gapBefore) {
                            ctx.fillStyle = Kirigami.Theme.disabledTextColor.toString()
                            ctx.fillRect(Math.max(0, bx - 3), 0, 6, height)
                        }
                        if (samples[b].throttled || samples[b].severity === 2) {
                            ctx.fillStyle = samples[b].throttled
                                            ? Kirigami.Theme.negativeTextColor.toString()
                                            : Kirigami.Theme.neutralTextColor.toString()
                            ctx.fillRect(Math.max(0, bx - 1), 0, 3, height)
                        }
                    }
                    ctx.globalAlpha = 1

                    ctx.strokeStyle = Kirigami.Theme.disabledTextColor.toString()
                    ctx.lineWidth = 1
                    for (var g = 1; g <= 3; ++g) {
                        var gy = (height / 4) * g
                        ctx.beginPath()
                        ctx.moveTo(0, gy)
                        ctx.lineTo(width, gy)
                        ctx.stroke()
                    }

                    if (flightPage.showTempTrace)
                        drawTrace(ctx, samples, firstTime, timeSpan, "cpuTemp", "cpuTempAvailable", 25, 100, Kirigami.Theme.highlightColor)
                    if (flightPage.showLoadTrace)
                        drawTrace(ctx, samples, firstTime, timeSpan, "cpuLoad", "cpuLoadAvailable", 0, 100, Kirigami.Theme.neutralTextColor)
                    if (flightPage.showPsiTrace)
                        drawTrace(ctx, samples, firstTime, timeSpan, "responsivenessScore", "psiAvailable", 0, 100, Kirigami.Theme.positiveTextColor)
                    if (flightPage.showWattsTrace)
                        drawTrace(ctx, samples, firstTime, timeSpan, "systemWatts", "systemPowerAvailable", 0, 65, Kirigami.Theme.linkColor)

                    var selIdx = flightPage.controller.selectedHistoryIndex
                    if (selIdx >= 0 && selIdx < samples.length) {
                        var sx = ((samples[selIdx].timestampMs - firstTime) / timeSpan) * width
                        ctx.strokeStyle = Kirigami.Theme.highlightColor.toString()
                        ctx.lineWidth = 2
                        ctx.beginPath()
                        ctx.moveTo(sx, 0)
                        ctx.lineTo(sx, height)
                        ctx.stroke()
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    function selectAtX(mouseX) {
                        var samples = flightPage.controller.historySamples
                        if (!samples || samples.length === 0) return
                        var ratio = Math.min(1.0, Math.max(0.0, mouseX / Math.max(1.0, width)))
                        var targetTime = samples[0].timestampMs
                                + ratio * (samples[samples.length - 1].timestampMs - samples[0].timestampMs)
                        var nearest = 0
                        for (var i = 1; i < samples.length; ++i) {
                            if (Math.abs(samples[i].timestampMs - targetTime)
                                    < Math.abs(samples[nearest].timestampMs - targetTime)) nearest = i
                        }
                        flightPage.controller.selectedHistorySampleId = samples[nearest].sampleId
                    }
                    onPressed: (mouse) => { timelineCanvas.forceActiveFocus(); selectAtX(mouse.x) }
                    onPositionChanged: (mouse) => { if (pressed) selectAtX(mouse.x) }
                }
            }

            QQC2.Label {
                anchors.centerIn: parent
                visible: flightPage.controller.historySamples.length === 0
                text: "No history has been recorded yet"
                color: Kirigami.Theme.disabledTextColor
            }

        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: flightPage.controller.historySamples.length
                  ? "Recorded time: " + new Date(flightPage.controller.historySamples[0].timestampMs).toLocaleTimeString()
                    + " — " + new Date(flightPage.controller.historySamples[flightPage.controller.historySamples.length - 1].timestampMs).toLocaleTimeString()
                    + " · Select a sample by clicking the graph or using Left/Right arrow keys."
                  : "No history yet. Measurements appear here after the next sample."
            color: Kirigami.Theme.disabledTextColor
        }

        // Snapshot Inspector Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: inspectorCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: 10
            color: Kirigami.Theme.alternateBackgroundColor
            border.width: 1.5
            border.color: Kirigami.Theme.highlightColor

            ColumnLayout {
                id: inspectorCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.mediumSpacing

                    Kirigami.Heading {
                        level: 3
                        text: "Inspected moment: "
                              + (flightPage.controller.selectedHistoryDetail.timeString || "--:--:--")
                              + (flightPage.controller.selectedHistorySampleId === 0 ? " (Live)"
                                 : flightPage.controller.selectedHistoryExpired ? " (Frozen snapshot · expired from chart)" : " (Frozen historical sample)")
                        color: Kirigami.Theme.textColor
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    QQC2.Button { text: "Open System Monitor"; icon.name: "utilities-system-monitor"; onClicked: flightPage.controller.openSystemMonitor() }
                }

                QQC2.Label {
                    text: "Status at this time: " + (flightPage.controller.selectedHistoryDetail.verdictHeadline || "No recorded sample")
                    color: Kirigami.Theme.highlightColor
                    font.bold: true
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: width < 620 ? 1 : 2
                    rowSpacing: 8
                    columnSpacing: 16

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "CPU temperature: " + (flightPage.controller.selectedHistoryDetail.cpuTempAvailable ? Number(flightPage.controller.selectedHistoryDetail.cpuTemp).toFixed(1) + " °C" : "Unavailable")
                        color: Kirigami.Theme.textColor
                        font.bold: true
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "NVMe temperature: " + (flightPage.controller.selectedHistoryDetail.nvmeTempAvailable ? Number(flightPage.controller.selectedHistoryDetail.nvmeTemp).toFixed(1) + " °C" : "Unavailable")
                        color: Kirigami.Theme.textColor
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "CPU load: " + (flightPage.controller.selectedHistoryDetail.cpuLoadAvailable
                              ? Number(flightPage.controller.selectedHistoryDetail.cpuLoad).toFixed(1) + "%"
                                + (flightPage.controller.selectedHistoryDetail.cpuFrequencyAvailable
                                   ? " at " + flightPage.controller.selectedHistoryDetail.cpuFreqMhz + " MHz" : "")
                              : "Unavailable")
                        color: Kirigami.Theme.textColor
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "System power: " + (flightPage.controller.selectedHistoryDetail.systemPowerAvailable
                              ? Number(flightPage.controller.selectedHistoryDetail.systemWatts).toFixed(1) + " W"
                                + (flightPage.controller.selectedHistoryDetail.systemPowerEstimated ? " · Estimated" : " · Measured")
                              : "Unavailable")
                        color: Kirigami.Theme.textColor
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "Concurrent process: " + (flightPage.controller.selectedHistoryDetail.culpritName || "Unavailable")
                              + (flightPage.controller.selectedHistoryDetail.culpritName
                                 ? " (PID " + flightPage.controller.selectedHistoryDetail.culpritPid + ")" : "")
                        color: Kirigami.Theme.neutralTextColor
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: flightPage.controller.selectedHistoryDetail.culpritName
                              ? "Observed use: " + Number(flightPage.controller.selectedHistoryDetail.culpritCpuPercent).toFixed(1)
                                + "% CPU · " + Math.round(Number(flightPage.controller.selectedHistoryDetail.culpritRssMb)) + " MiB RSS"
                              : "Observed process use: Unavailable"
                        color: Kirigami.Theme.neutralTextColor
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "Calculated pressure index: " + (flightPage.controller.selectedHistoryDetail.psiAvailable ? Number(flightPage.controller.selectedHistoryDetail.responsivenessScore).toFixed(1) + "%" : "Unavailable")
                        color: Kirigami.Theme.positiveTextColor
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "New throttle events: " + (!flightPage.controller.selectedHistoryDetail.throttleCountersAvailable
                              ? "Unavailable" : (flightPage.controller.selectedHistoryDetail.throttled ? "Observed" : "None observed"))
                        color: Kirigami.Theme.textColor
                    }
                }

                QQC2.GroupBox {
                    Layout.fillWidth: true
                    title: "Recorded data quality"
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        Repeater {
                            model: ["cpuLoad", "cpuTemperature", "pressureIndex", "packagePower"]
                            delegate: QQC2.Label {
                                required property string modelData
                                readonly property var quality: flightPage.controller.selectedHistoryDetail.metricMetadata
                                                               ? flightPage.controller.selectedHistoryDetail.metricMetadata[modelData] : null
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
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
