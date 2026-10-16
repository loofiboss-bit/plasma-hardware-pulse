import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

Item {
    id: fullRoot

    required property var controller

    Layout.minimumWidth: Kirigami.Units.gridUnit * 24
    Layout.preferredWidth: Kirigami.Units.gridUnit * 26
    Layout.minimumHeight: Kirigami.Units.gridUnit * 25
    Layout.preferredHeight: Kirigami.Units.gridUnit * 27

    function severityColor(sev) {
        if (sev >= 3) return "#ef4444"
        if (sev === 2) return "#f97316"
        if (sev === 1) return "#f59e0b"
        return "#10b981"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.mediumSpacing

        // Header Row
        RowLayout {
            Layout.fillWidth: true

            Kirigami.Heading {
                level: 3
                text: "Loofi Pulse — Causal Telemetry"
                Layout.fillWidth: true
            }

            PlasmaComponents3.Button {
                icon.name: "edit-copy"
                text: "Copy Report"
                onClicked: fullRoot.controller.copyIncidentReportToClipboard()
            }

            PlasmaComponents3.Button {
                icon.name: "window-new"
                text: "Open Studio"
                onClicked: fullRoot.controller.launchStudioApp()
            }
        }

        // 1. Causal Diagnosis Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: verdictCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: 10
            color: Qt.rgba(0.09, 0.13, 0.21, 0.85)
            border.width: 1.5
            border.color: fullRoot.severityColor(fullRoot.controller.verdictSeverity)

            ColumnLayout {
                id: verdictCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: fullRoot.severityColor(fullRoot.controller.verdictSeverity)
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        text: fullRoot.controller.verdictHeadline
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.controller.verdictCausalChain
                    font.family: "monospace"
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    opacity: 0.9
                    wrapMode: Text.WordWrap
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.controller.verdictRecommendation
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    opacity: 0.75
                    wrapMode: Text.WordWrap
                }
            }
        }

        // 2. Physical Domain Cards Grid (2x2)
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: Kirigami.Units.smallSpacing
            columnSpacing: Kirigami.Units.smallSpacing

            // CPU & SoC Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 82
                radius: 8
                color: Qt.rgba(1, 1, 1, 0.05)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: 2

                    PlasmaComponents3.Label {
                        text: "CPU & SoC Package"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.75
                    }
                    PlasmaComponents3.Label {
                        text: Math.round(fullRoot.controller.cpuPackageTemp) + "°C  •  "
                              + Math.round(fullRoot.controller.cpuLoadPercent) + "%"
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 2
                    }
                    PlasmaComponents3.Label {
                        text: fullRoot.controller.cpuAvgFreqMhz + " MHz  |  Headroom: "
                              + Math.round(fullRoot.controller.thermalHeadroom) + "°C"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.8
                    }
                }
            }

            // PSI & ZRAM Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 82
                radius: 8
                color: Qt.rgba(1, 1, 1, 0.05)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: 2

                    PlasmaComponents3.Label {
                        text: "PSI Responsiveness & ZRAM"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.75
                    }
                    PlasmaComponents3.Label {
                        text: fullRoot.controller.responsivenessScore.toFixed(1) + "%  •  "
                              + fullRoot.controller.zramCompressionRatio.toFixed(1) + "× ZRAM"
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 2
                    }
                    PlasmaComponents3.Label {
                        text: "Stall C:" + fullRoot.controller.psiCpuSome10.toFixed(1)
                              + "% M:" + fullRoot.controller.psiMemSome10.toFixed(1)
                              + "% IO:" + fullRoot.controller.psiIoFull10.toFixed(1) + "%"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.8
                    }
                }
            }

            // Storage & Wireless Thermal Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 82
                radius: 8
                color: Qt.rgba(1, 1, 1, 0.05)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: 2

                    PlasmaComponents3.Label {
                        text: "NVMe & Wireless"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.75
                    }
                    PlasmaComponents3.Label {
                        text: "NVMe " + Math.round(fullRoot.controller.nvmeTempCelsius) + "°C  •  WiFi "
                              + Math.round(fullRoot.controller.wifiTempCelsius) + "°C"
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 1
                    }
                    PlasmaComponents3.Label {
                        text: fullRoot.controller.gpuAvailable
                              ? ("GPU: " + fullRoot.controller.gpuCurrentFreqMhz + " MHz (" + Math.round(fullRoot.controller.gpuBusyPercent) + "%)")
                              : "RAM: " + fullRoot.controller.ramUsedGib.toFixed(1) + " / " + fullRoot.controller.ramTotalGib.toFixed(1) + " GiB"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.8
                    }
                }
            }

            // Energy Flow Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 82
                radius: 8
                color: Qt.rgba(1, 1, 1, 0.05)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: 2

                    PlasmaComponents3.Label {
                        text: "Energy Flow & Envelope"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.75
                    }
                    PlasmaComponents3.Label {
                        text: "~" + fullRoot.controller.packageWatts.toFixed(1) + " W  •  "
                              + fullRoot.controller.batteryTimeRemainingText
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 2
                    }
                    PlasmaComponents3.Label {
                        text: "PL1/2: " + Math.round(fullRoot.controller.pl1LimitWatts) + "/"
                              + Math.round(fullRoot.controller.pl2LimitWatts) + "W  |  Fan: "
                              + (fullRoot.controller.primaryFanRpm > 0 ? (fullRoot.controller.primaryFanRpm + " RPM") : "0 RPM")
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.8
                    }
                }
            }
        }

        // 3. Mini Live Sparkline (Temperature & Load)
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 70
            radius: 8
            color: Qt.rgba(0.05, 0.08, 0.14, 0.85)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.1)

            Canvas {
                id: miniSparkCanvas
                anchors.fill: parent
                anchors.margins: 8

                Connections {
                    target: fullRoot.controller
                    function onTelemetryUpdated() {
                        miniSparkCanvas.requestPaint()
                    }
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    var samples = fullRoot.controller.historySamples
                    if (!samples || samples.length < 2) return

                    ctx.lineWidth = 2
                    ctx.strokeStyle = "#06b6d4"
                    ctx.beginPath()
                    for (var i = 0; i < samples.length; ++i) {
                        var x = (i / Math.max(1, samples.length - 1)) * width
                        var tempNorm = Math.min(1.0, Math.max(0.0, (samples[i].cpuTemp - 30.0) / 70.0))
                        var y = height - (tempNorm * (height - 8)) - 4
                        if (i === 0) ctx.moveTo(x, y)
                        else ctx.lineTo(x, y)
                    }
                    ctx.stroke()
                }
            }

            PlasmaComponents3.Label {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 6
                text: "Thermal Curve (30–100°C)  •  Top Process: "
                      + (fullRoot.controller.culpritName || "idle")
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                opacity: 0.75
            }
        }

        // 4. Power Profile Quick Switcher
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents3.Label {
                text: "Power Profile:"
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                opacity: 0.85
            }

            Repeater {
                model: ["power-saver", "balanced", "performance"]
                delegate: PlasmaComponents3.Button {
                    required property string modelData
                    Layout.fillWidth: true
                    text: modelData
                    checked: fullRoot.controller.activePowerProfile === modelData
                    checkable: true
                    onClicked: fullRoot.controller.setPowerProfile(modelData)
                }
            }
        }
    }
}
