import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: flightPage
    title: "Flight Recorder — Causal Time Machine (5 min history)"

    required property var controller

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.mediumSpacing

            QQC2.Button {
                icon.name: "dialog-warning"
                text: "Jump to Peak Thermal Spike"
                onClicked: flightPage.controller.jumpToPeakThermalSpike()
            }

            QQC2.Button {
                icon.name: "chronometer"
                text: "Jump to Worst PSI Stall"
                onClicked: flightPage.controller.jumpToWorstStallSpike()
            }

            QQC2.Button {
                icon.name: "media-playback-start"
                text: "Follow Live (Latest Sample)"
                onClicked: flightPage.controller.selectedHistoryIndex = -1
            }

            Item { Layout.fillWidth: true }

            QQC2.Label {
                text: "Recorded samples: " + flightPage.controller.historySamples.length + " / 300s"
                color: "#94a3b8"
            }
        }

        // Interactive Multi-Channel Canvas Timeline
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 280
            radius: 12
            color: Qt.rgba(0.06, 0.09, 0.16, 0.95)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.15)

            Canvas {
                id: timelineCanvas
                anchors.fill: parent
                anchors.margins: 16

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

                    // 2. CPU Load Curve (Amber)
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
                anchors.margins: 14
                spacing: 16

                QQC2.Label { text: "━ CPU Temp (°C)"; color: "#06b6d4"; font.bold: true }
                QQC2.Label { text: "━ CPU Load (%)"; color: "#f59e0b"; font.bold: true }
                QQC2.Label { text: "Click or drag on the chart to inspect any historical timestamp"; color: "#94a3b8" }
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

                Kirigami.Heading {
                    level: 3
                    text: "Inspected Moment: "
                          + (flightPage.controller.selectedHistoryDetail.timeString || "--:--:--")
                          + (flightPage.controller.selectedHistoryIndex < 0 ? " (LIVE)" : " (Historical Lock)")
                    color: "#f8fafc"
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
                        text: "CPU Temp: " + Number(flightPage.controller.selectedHistoryDetail.cpuTemp || 0).toFixed(1) + "°C"
                        color: "#e2e8f0"
                        font.bold: true
                    }
                    QQC2.Label {
                        text: "NVMe Temp: " + Number(flightPage.controller.selectedHistoryDetail.nvmeTemp || 0).toFixed(1) + "°C"
                        color: "#e2e8f0"
                    }
                    QQC2.Label {
                        text: "CPU Load & Clock: " + Number(flightPage.controller.selectedHistoryDetail.cpuLoad || 0).toFixed(1)
                              + "% @ " + (flightPage.controller.selectedHistoryDetail.cpuFreqMhz || 0) + " MHz"
                        color: "#e2e8f0"
                    }
                    QQC2.Label {
                        text: "Power Draw: ~" + Number(flightPage.controller.selectedHistoryDetail.systemWatts || 0).toFixed(1) + " W"
                        color: "#e2e8f0"
                    }
                    QQC2.Label {
                        text: "Driving Process: " + (flightPage.controller.selectedHistoryDetail.culpritName || "None")
                              + " (PID " + (flightPage.controller.selectedHistoryDetail.culpritPid || 0) + ")"
                        color: "#f59e0b"
                        font.bold: true
                    }
                    QQC2.Label {
                        text: "Process CPU Share: " + Number(flightPage.controller.selectedHistoryDetail.culpritCpuPercent || 0).toFixed(1) + "%"
                        color: "#f59e0b"
                    }
                    QQC2.Label {
                        text: "PSI Responsiveness: " + Number(flightPage.controller.selectedHistoryDetail.responsivenessScore || 100).toFixed(1) + "%"
                        color: "#10b981"
                    }
                    QQC2.Label {
                        text: "Throttling Flag: " + (flightPage.controller.selectedHistoryDetail.throttled ? "YES (Active)" : "No")
                        color: flightPage.controller.selectedHistoryDetail.throttled ? "#ef4444" : "#94a3b8"
                    }
                }
            }
        }
    }
}
