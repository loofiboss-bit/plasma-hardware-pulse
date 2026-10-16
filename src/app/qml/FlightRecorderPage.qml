import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: flightPage
    title: "Flight Recorder — Causal Time Machine (5 min history)"

    required property var controller
    property bool showTempTrace: true
    property bool showLoadTrace: true
    property bool showPsiTrace: true
    property bool showWattsTrace: false

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
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
                onClicked: flightPage.controller.selectedHistoryIndex = -1
            }

            QQC2.Button {
                icon.name: flightPage.controller.recordingPaused ? "media-playback-start" : "media-playback-pause"
                text: flightPage.controller.recordingPaused ? "Resume Recording" : "Pause Recording"
                checkable: true
                checked: flightPage.controller.recordingPaused
                onClicked: flightPage.controller.recordingPaused = !flightPage.controller.recordingPaused
            }

            Item { Layout.fillWidth: true }

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

            QQC2.Label {
                text: flightPage.controller.historySamples.length + " / 300s"
                color: "#94a3b8"
                font.bold: true
            }
        }

        // Interactive Multi-Channel Canvas Timeline
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 295
            radius: 12
            color: Qt.rgba(0.06, 0.09, 0.16, 0.95)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.15)

            Canvas {
                id: timelineCanvas
                anchors.fill: parent
                anchors.topMargin: 38
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.bottomMargin: 16

                Connections {
                    target: flightPage.controller
                    function onTelemetryUpdated() { timelineCanvas.requestPaint() }
                    function onSelectedHistoryChanged() { timelineCanvas.requestPaint() }
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    var samples = flightPage.controller.historySamples
                    if (!samples || samples.length < 2) return

                    // 0. Highlight Throttle / Bottleneck Incident Bands
                    for (var b = 0; b < samples.length; ++b) {
                        if (samples[b].throttled || samples[b].severity >= 2) {
                            var bx = (b / Math.max(1, samples.length - 1)) * width
                            ctx.fillStyle = samples[b].throttled ? "rgba(239, 68, 68, 0.22)" : "rgba(249, 115, 22, 0.16)"
                            ctx.fillRect(Math.max(0, bx - 2), 0, 5, height)
                        }
                    }

                    // Grid lines
                    ctx.strokeStyle = "rgba(255, 255, 255, 0.08)"
                    ctx.lineWidth = 1
                    for (var g = 1; g <= 3; ++g) {
                        var gy = (height / 4) * g
                        ctx.beginPath()
                        ctx.moveTo(0, gy)
                        ctx.lineTo(width, gy)
                        ctx.stroke()
                    }

                    // 1. CPU Temperature Curve (Cyan)
                    if (flightPage.showTempTrace) {
                        ctx.strokeStyle = "#06b6d4"
                        ctx.lineWidth = 2.5
                        ctx.beginPath()
                        for (var i = 0; i < samples.length; ++i) {
                            var x = (i / Math.max(1, samples.length - 1)) * width
                            var tNorm = Math.min(1.0, Math.max(0.0, (samples[i].cpuTemp - 25.0) / 75.0))
                            var y = height - tNorm * (height - 12) - 6
                            if (i === 0) ctx.moveTo(x, y)
                            else ctx.lineTo(x, y)
                        }
                        ctx.stroke()
                    }

                    // 2. CPU Load Curve (Amber)
                    if (flightPage.showLoadTrace) {
                        ctx.strokeStyle = "#f59e0b"
                        ctx.lineWidth = 1.8
                        ctx.beginPath()
                        for (var j = 0; j < samples.length; ++j) {
                            var lx = (j / Math.max(1, samples.length - 1)) * width
                            var lNorm = Math.min(1.0, Math.max(0.0, samples[j].cpuLoad / 100.0))
                            var ly = height - lNorm * (height - 12) - 6
                            if (j === 0) ctx.moveTo(lx, ly)
                            else ctx.lineTo(lx, ly)
                        }
                        ctx.stroke()
                    }

                    // 3. PSI Responsiveness Curve (Emerald)
                    if (flightPage.showPsiTrace) {
                        ctx.strokeStyle = "#10b981"
                        ctx.lineWidth = 1.8
                        ctx.beginPath()
                        for (var k = 0; k < samples.length; ++k) {
                            var px = (k / Math.max(1, samples.length - 1)) * width
                            var pNorm = Math.min(1.0, Math.max(0.0, samples[k].responsivenessScore / 100.0))
                            var py = height - pNorm * (height - 12) - 6
                            if (k === 0) ctx.moveTo(px, py)
                            else ctx.lineTo(px, py)
                        }
                        ctx.stroke()
                    }

                    // 4. System Power Watts Curve (Purple, scaled 0-65W)
                    if (flightPage.showWattsTrace) {
                        ctx.strokeStyle = "#a855f7"
                        ctx.lineWidth = 1.8
                        ctx.beginPath()
                        for (var w = 0; w < samples.length; ++w) {
                            var wx = (w / Math.max(1, samples.length - 1)) * width
                            var wNorm = Math.min(1.0, Math.max(0.0, samples[w].systemWatts / 65.0))
                            var wy = height - wNorm * (height - 12) - 6
                            if (w === 0) ctx.moveTo(wx, wy)
                            else ctx.lineTo(wx, wy)
                        }
                        ctx.stroke()
                    }

                    // Selected cursor line
                    var selIdx = flightPage.controller.selectedHistoryIndex
                    if (selIdx >= 0 && selIdx < samples.length) {
                        var sx = (selIdx / Math.max(1, samples.length - 1)) * width
                        ctx.strokeStyle = "#f43f5e"
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
                        var idx = Math.round(ratio * (samples.length - 1))
                        flightPage.controller.selectedHistoryIndex = idx
                    }
                    onPressed: (mouse) => selectAtX(mouse.x)
                    onPositionChanged: (mouse) => { if (pressed) selectAtX(mouse.x) }
                }
            }

            RowLayout {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 10
                spacing: 12

                QQC2.CheckBox {
                    text: "━ CPU Temp (°C)"
                    checked: flightPage.showTempTrace
                    onToggled: { flightPage.showTempTrace = checked; timelineCanvas.requestPaint() }
                }
                QQC2.CheckBox {
                    text: "━ CPU Load (%)"
                    checked: flightPage.showLoadTrace
                    onToggled: { flightPage.showLoadTrace = checked; timelineCanvas.requestPaint() }
                }
                QQC2.CheckBox {
                    text: "━ PSI Score (%)"
                    checked: flightPage.showPsiTrace
                    onToggled: { flightPage.showPsiTrace = checked; timelineCanvas.requestPaint() }
                }
                QQC2.CheckBox {
                    text: "━ Power (W)"
                    checked: flightPage.showWattsTrace
                    onToggled: { flightPage.showWattsTrace = checked; timelineCanvas.requestPaint() }
                }
                Item { Layout.fillWidth: true }
                QQC2.Label {
                    text: "Red/Orange bands = Throttling / Bottleneck • Click/drag to inspect"
                    color: "#94a3b8"
                    font.pixelSize: 12
                }
            }
        }

        // Snapshot Inspector Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: inspectorCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: 10
            color: Qt.rgba(0.10, 0.14, 0.23, 0.90)
            border.width: 1.5
            border.color: "#38bdf8"

            ColumnLayout {
                id: inspectorCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.mediumSpacing

                    Kirigami.Heading {
                        level: 3
                        text: "Inspected Moment: "
                              + (flightPage.controller.selectedHistoryDetail.timeString || "--:--:--")
                              + (flightPage.controller.selectedHistoryIndex < 0 ? " (LIVE)" : " (Historical Lock)")
                        color: "#f8fafc"
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    QQC2.Button {
                        visible: (flightPage.controller.selectedHistoryDetail.culpritPid || 0) > 1
                        icon.name: "process-stop"
                        text: "Terminate " + (flightPage.controller.selectedHistoryDetail.culpritName || "Process")
                              + " (" + (flightPage.controller.selectedHistoryDetail.culpritPid || 0) + ")"
                        onClicked: flightPage.controller.terminateProcess(
                                       flightPage.controller.selectedHistoryDetail.culpritPid,
                                       flightPage.controller.selectedHistoryDetail.culpritName)
                    }
                }

                QQC2.Label {
                    text: "Causal Status at Timestamp: " + (flightPage.controller.selectedHistoryDetail.verdictHeadline || "")
                    color: "#38bdf8"
                    font.bold: true
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    rowSpacing: 8
                    columnSpacing: 16

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "CPU Temp: " + Number(flightPage.controller.selectedHistoryDetail.cpuTemp || 0).toFixed(1) + "°C"
                        color: "#e2e8f0"
                        font.bold: true
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "NVMe Temp: " + Number(flightPage.controller.selectedHistoryDetail.nvmeTemp || 0).toFixed(1) + "°C"
                        color: "#e2e8f0"
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "CPU Load: " + Number(flightPage.controller.selectedHistoryDetail.cpuLoad || 0).toFixed(1)
                              + "% @ " + (flightPage.controller.selectedHistoryDetail.cpuFreqMhz || 0) + " MHz"
                        color: "#e2e8f0"
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "Power Draw: ~" + Number(flightPage.controller.selectedHistoryDetail.systemWatts || 0).toFixed(1) + " W"
                        color: "#e2e8f0"
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "Driving: " + (flightPage.controller.selectedHistoryDetail.culpritName || "None")
                              + " (PID " + (flightPage.controller.selectedHistoryDetail.culpritPid || 0) + ")"
                        color: "#f59e0b"
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "Share: " + Number(flightPage.controller.selectedHistoryDetail.culpritCpuPercent || 0).toFixed(1)
                              + "% CPU • " + Math.round(Number(flightPage.controller.selectedHistoryDetail.culpritRssMb || 0)) + " MiB"
                        color: "#f59e0b"
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "PSI Responsiveness: " + Number(flightPage.controller.selectedHistoryDetail.responsivenessScore || 100).toFixed(1) + "%"
                        color: "#10b981"
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "Throttling: " + (flightPage.controller.selectedHistoryDetail.throttled ? "YES (Active)" : "No")
                        color: flightPage.controller.selectedHistoryDetail.throttled ? "#ef4444" : "#94a3b8"
                    }
                }
            }
        }
    }
}
