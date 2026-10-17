pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

Item {
    id: fullRoot

    required property var controller

    Layout.minimumWidth: Kirigami.Units.gridUnit * 16
    Layout.preferredWidth: Kirigami.Units.gridUnit * 26
    Layout.minimumHeight: Kirigami.Units.gridUnit * 20
    Layout.preferredHeight: Kirigami.Units.gridUnit * 27

    function severityColor(sev) {
        if (sev === 4) return Kirigami.Theme.neutralTextColor
        if (sev === 3) return Kirigami.Theme.negativeTextColor
        if (sev === 2) return Kirigami.Theme.neutralTextColor
        if (sev === 1) return Kirigami.Theme.neutralTextColor
        return Kirigami.Theme.positiveTextColor
    }

    function qualityText(key, label) {
        const metadata = controller.metricMetadata || {}
        const item = metadata[key]
        if (!item) return label + ": Unavailable — awaiting a sample."
        const state = item.state === "available" ? "Measured"
                    : item.state === "calculated" ? "Calculated"
                    : item.state === "estimated" ? "Estimated"
                    : item.state === "warming-up" ? "Warming up"
                    : "Unavailable"
        const explanation = item.detail || (state === "Unavailable" ? "No valid reading from this source." : "")
        const sampledAt = item.sampledAtMs > 0
                        ? Qt.formatTime(new Date(item.sampledAtMs), "HH:mm:ss")
                        : "Awaiting first sample"
        return label + ": " + state + (explanation ? " — " + explanation : "")
               + " · Source: " + (item.source || "Unknown") + " · Sample: " + sampledAt
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
        id: contentColumn
        width: parent.width - Kirigami.Units.largeSpacing * 2
        x: Kirigami.Units.largeSpacing
        y: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.mediumSpacing

        // Header Row
        ColumnLayout {
            Layout.fillWidth: true

            Kirigami.Heading {
                level: 3
                text: "Loofi Pulse"
                Layout.fillWidth: true
            }

            Flow {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

            PlasmaComponents3.Button {
                icon.name: "edit-copy"
                text: "Copy report"
                Accessible.name: "Copy current incident report"
                onClicked: fullRoot.controller.copyIncidentReportToClipboard()
            }

            PlasmaComponents3.Button {
                icon.name: "window-new"
                text: "Open app"
                Accessible.name: "Open Loofi Pulse app"
                onClicked: fullRoot.controller.launchStudioApp()
            }
            PlasmaComponents3.Button {
                icon.name: "utilities-system-monitor"
                text: "Open System Monitor"
                Accessible.name: "Open KDE System Monitor process view"
                onClicked: fullRoot.controller.openSystemMonitor()
            }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: fullRoot.controller.lastActionStatus.length > 0 || fullRoot.controller.lastActionPath.length > 0
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                text: fullRoot.controller.lastActionStatus
                wrapMode: Text.WordWrap
                color: fullRoot.controller.lastActionSucceeded
                       ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor
                Accessible.name: "Action result: " + text
            }
            PlasmaComponents3.Button {
                visible: fullRoot.controller.lastActionSucceeded && fullRoot.controller.lastActionPath.length > 0
                text: "Open folder"
                icon.name: "folder-open"
                onClicked: fullRoot.controller.openReportsFolder()
            }
        }

        // 1. Causal Diagnosis Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: verdictCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: 10
            color: Kirigami.Theme.backgroundColor
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
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        radius: 5
                        color: fullRoot.severityColor(fullRoot.controller.verdictSeverity)
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        text: "Status: " + fullRoot.controller.verdictHeadline
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.controller.verdictCausalChain
                    font.family: Kirigami.Theme.defaultFont.family
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    opacity: 0.9
                    wrapMode: Text.WordWrap
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: "Next step: " + fullRoot.controller.verdictRecommendation
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    opacity: 0.75
                    wrapMode: Text.WordWrap
                }
            }
        }

        // 2. Physical Domain Cards Grid (2x2)
        GridLayout {
            Layout.fillWidth: true
            columns: width >= Kirigami.Units.gridUnit * 25 ? 2 : 1
            rowSpacing: Kirigami.Units.smallSpacing
            columnSpacing: Kirigami.Units.smallSpacing

            // CPU & SoC Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: cpuCardColumn.implicitHeight + Kirigami.Units.mediumSpacing * 2
                radius: 8
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: Kirigami.Theme.alternateBackgroundColor

                ColumnLayout {
                    id: cpuCardColumn
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: 2

                    PlasmaComponents3.Label {
                        text: "CPU & SoC Package"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.75
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: (fullRoot.controller.cpuTempAvailable ? Math.round(fullRoot.controller.cpuPackageTemp) + "°C" : "Temp unavailable") + "  •  "
                              + (fullRoot.controller.cpuLoadAvailable ? Math.round(fullRoot.controller.cpuLoadPercent) + "%" : "Load unavailable")
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 2
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Thermal margin: "
                              + (fullRoot.controller.thermalHeadroomAvailable ? Math.round(fullRoot.controller.thermalHeadroom) + "°C" : "Unavailable")
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.8
                    }
                }
            }

            // PSI & ZRAM Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: pressureCardColumn.implicitHeight + Kirigami.Units.mediumSpacing * 2
                radius: 8
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: Kirigami.Theme.alternateBackgroundColor

                ColumnLayout {
                    id: pressureCardColumn
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: 2

                    PlasmaComponents3.Label {
                        text: "Memory & pressure"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.75
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Calculated pressure index: " + (fullRoot.controller.psiAvailable ? fullRoot.controller.responsivenessScore.toFixed(1) + "%" : "Unavailable")
                              + " · ZRAM: " + (fullRoot.controller.zramCompressionAvailable
                              ? fullRoot.controller.zramCompressionRatio.toFixed(1) + "× compression"
                              : (fullRoot.controller.zramAvailable ? "No compressed pages in use" : "Unavailable"))
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 2
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Time tasks waited · CPU " + (fullRoot.controller.psiCpuAvailable ? fullRoot.controller.psiCpuSome10.toFixed(1) + "%" : "Unavailable")
                              + " · Memory " + (fullRoot.controller.psiMemoryAvailable ? fullRoot.controller.psiMemSome10.toFixed(1) + "%" : "Unavailable")
                              + " · I/O " + (fullRoot.controller.psiIoFullAvailable ? fullRoot.controller.psiIoFull10.toFixed(1) + "%" : "Unavailable")
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.8
                    }
                }
            }

            // Storage & Wireless Thermal Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: deviceCardColumn.implicitHeight + Kirigami.Units.mediumSpacing * 2
                radius: 8
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: Kirigami.Theme.alternateBackgroundColor

                ColumnLayout {
                    id: deviceCardColumn
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: 2

                    PlasmaComponents3.Label {
                        text: "NVMe & Wireless"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.75
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "NVMe " + (fullRoot.controller.nvmeTempAvailable ? Math.round(fullRoot.controller.nvmeTempCelsius) + "°C" : "Unavailable")
                              + " · Wi-Fi " + (fullRoot.controller.wifiTempAvailable ? Math.round(fullRoot.controller.wifiTempCelsius) + "°C" : "Unavailable")
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 1
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "GPU load: " + (fullRoot.controller.gpuBusyAvailable ? Math.round(fullRoot.controller.gpuBusyPercent) + "%" : "Unavailable")
                              + " · GPU temp: " + (fullRoot.controller.gpuTempAvailable ? Math.round(fullRoot.controller.gpuTempCelsius) + "°C" : "Unavailable")
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.8
                    }
                }
            }

            // Energy Flow Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: powerCardColumn.implicitHeight + Kirigami.Units.mediumSpacing * 2
                radius: 8
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: Kirigami.Theme.alternateBackgroundColor

                ColumnLayout {
                    id: powerCardColumn
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: 2

                    PlasmaComponents3.Label {
                        text: "Energy Flow & Envelope"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.75
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: (fullRoot.controller.packagePowerAvailable ? fullRoot.controller.packageWatts.toFixed(1) + " W" + (fullRoot.controller.packagePowerEstimated ? " · Estimated" : " · Measured") : "Power unavailable") + "  •  "
                              + (fullRoot.controller.batteryStateAvailable ? fullRoot.controller.batteryTimeRemainingText : "Battery unavailable")
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 2
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Power limits: " + (fullRoot.controller.powerLimitsAvailable
                              ? Math.round(fullRoot.controller.pl1LimitWatts) + "/" + Math.round(fullRoot.controller.pl2LimitWatts) + " W"
                              : "Unavailable") + " · Fan: "
                              + (fullRoot.controller.fanAvailable ? (fullRoot.controller.primaryFanRpm + " RPM") : "Fan unavailable")
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.8
                    }
                }
            }
        }

        // 3. Mini Live Sparkline (Temperature & Load)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 5
            radius: 8
            color: Kirigami.Theme.alternateBackgroundColor
            border.width: 1
            border.color: Kirigami.Theme.alternateBackgroundColor

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
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    const samples = fullRoot.controller.historySamples
                    if (!samples || samples.length < 2) return

                    const firstTime = samples[0].monotonicMs
                    const duration = Math.max(1, samples[samples.length - 1].monotonicMs - firstTime)
                    ctx.lineWidth = 2
                    ctx.strokeStyle = Kirigami.Theme.highlightColor
                    ctx.beginPath()
                    let drawing = false
                    for (let i = 0; i < samples.length; ++i) {
                        const sample = samples[i]
                        if (!sample.cpuTempAvailable) {
                            drawing = false
                            continue
                        }
                        const x = (sample.monotonicMs - firstTime) / duration * width
                        const tempNorm = Math.min(1, Math.max(0, (sample.cpuTemp - 30) / 70))
                        const y = height - (tempNorm * (height - 8)) - 4
                        if (!drawing || sample.gapBefore) ctx.moveTo(x, y)
                        else ctx.lineTo(x, y)
                        drawing = true
                    }
                    ctx.stroke()
                }
            }

            PlasmaComponents3.Label {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 6
                text: "CPU temperature · last 5 minutes · 30–100°C"
                font: Kirigami.Theme.smallFont
                opacity: 0.75
            }
        }

        // 4. Power Profile Quick Switcher
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents3.Label {
                Layout.fillWidth: true
                text: "Power profile: " + (fullRoot.controller.powerProfileChangePending ? "Changing…"
                      : (fullRoot.controller.activePowerProfile || "Unavailable"))
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                opacity: 0.85
            }

            Flow {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
            Repeater {
                model: fullRoot.controller.availablePowerProfiles
                delegate: PlasmaComponents3.Button {
                    required property string modelData
                    text: modelData
                    checked: fullRoot.controller.activePowerProfile === modelData
                    checkable: true
                    enabled: fullRoot.controller.powerProfileServiceAvailable && !fullRoot.controller.powerProfileChangePending
                    Accessible.name: "Select " + modelData + " power profile"
                    onClicked: fullRoot.controller.setPowerProfile(modelData)
                }
            }
            }
            PlasmaComponents3.Label {
                visible: !fullRoot.controller.powerProfileServiceAvailable
                text: "Power profile service unavailable"
            }
        }

        PlasmaComponents3.Button {
            id: qualityToggle
            Layout.fillWidth: true
            text: "Data quality"
            checkable: true
            Accessible.name: "Show data quality, sources, and sample times"
            Accessible.description: "Expand details for CPU load, CPU temperature, pressure index, and package power"
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: qualityToggle.checked
            spacing: Kirigami.Units.smallSpacing
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                text: fullRoot.qualityText("cpuLoad", "CPU load")
                wrapMode: Text.WordWrap
            }
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                text: fullRoot.qualityText("cpuTemperature", "CPU temperature")
                wrapMode: Text.WordWrap
            }
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                text: fullRoot.qualityText("pressureIndex", "Pressure index")
                wrapMode: Text.WordWrap
            }
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                text: fullRoot.qualityText("packagePower", "CPU package power")
                wrapMode: Text.WordWrap
            }
        }
        }
    }
}
