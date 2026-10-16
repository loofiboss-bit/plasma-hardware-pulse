import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: overviewPage
    title: "Causal Pulse & Physical Topology"

    required property var controller
    property int selectedDomainFilter: -1

    function severityColor(sev) {
        if (sev >= 3) return "#ef4444"
        if (sev === 2) return "#f97316"
        if (sev === 1) return "#f59e0b"
        return "#10b981"
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        // 1. Causal Diagnosis Engine Banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: bannerCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: 12
            color: Qt.rgba(0.08, 0.12, 0.20, 0.92)
            border.width: 2
            border.color: overviewPage.severityColor(overviewPage.controller.verdictSeverity)

            ColumnLayout {
                id: bannerCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.mediumSpacing

                    Rectangle {
                        width: 14
                        height: 14
                        radius: 7
                        color: overviewPage.severityColor(overviewPage.controller.verdictSeverity)
                    }

                    Kirigami.Heading {
                        level: 2
                        text: overviewPage.controller.verdictHeadline
                        color: "#f8fafc"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    QQC2.Label {
                        text: "Bottleneck Domain: " + overviewPage.controller.primaryBottleneckDomain
                        color: "#94a3b8"
                        font.bold: true
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: chainText.implicitHeight + 16
                    radius: 8
                    color: Qt.rgba(0, 0, 0, 0.35)

                    QQC2.Label {
                        id: chainText
                        anchors.fill: parent
                        anchors.margins: 8
                        text: "Causal Chain:  " + overviewPage.controller.verdictCausalChain
                        color: "#38bdf8"
                        font.family: "monospace"
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.mediumSpacing

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "Diagnosis & Action: " + overviewPage.controller.verdictRecommendation
                        color: "#cbd5e1"
                        wrapMode: Text.WordWrap
                    }

                    QQC2.Button {
                        visible: overviewPage.controller.culpritPid > 1 && overviewPage.controller.culpritCpuPercent >= 5.0
                        icon.name: "process-stop"
                        text: "Terminate " + overviewPage.controller.culpritName + " (" + overviewPage.controller.culpritPid + ")"
                        onClicked: overviewPage.controller.terminateProcess(overviewPage.controller.culpritPid, overviewPage.controller.culpritName)
                    }

                    QQC2.Button {
                        visible: overviewPage.controller.verdictSeverity >= 2 && overviewPage.controller.activePowerProfile === "performance"
                        icon.name: "battery-profile-balanced"
                        text: "Switch to Balanced"
                        onClicked: overviewPage.controller.setPowerProfile("balanced")
                    }

                    QQC2.Button {
                        icon.name: "edit-copy"
                        text: "Copy Report"
                        onClicked: overviewPage.controller.copyIncidentReportToClipboard()
                    }
                }
            }
        }

        // 2. Four Pillar Summary Row
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            // Pillar 1: Thermal Headroom & Velocity
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 110
                radius: 10
                color: Qt.rgba(0.11, 0.16, 0.25, 0.85)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: 4

                    QQC2.Label { text: "THERMAL HEADROOM"; color: "#94a3b8"; font.pixelSize: 11; font.bold: true }
                    QQC2.Label {
                        text: Math.round(overviewPage.controller.thermalHeadroom) + " °C left"
                        color: "#f8fafc"
                        font.pixelSize: 22
                        font.bold: true
                    }
                    QQC2.Label {
                        text: "Now: " + overviewPage.controller.cpuPackageTemp.toFixed(1) + "°C  •  dT/dt: "
                              + (overviewPage.controller.thermalVelocity >= 0 ? "+" : "")
                              + overviewPage.controller.thermalVelocity.toFixed(2) + "°C/s"
                        color: "#cbd5e1"
                        font.pixelSize: 12
                    }
                }
            }

            // Pillar 2: System PSI Responsiveness
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 110
                radius: 10
                color: Qt.rgba(0.11, 0.16, 0.25, 0.85)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: 4

                    QQC2.Label { text: "LINUX PSI RESPONSIVENESS"; color: "#94a3b8"; font.pixelSize: 11; font.bold: true }
                    QQC2.Label {
                        text: overviewPage.controller.responsivenessScore.toFixed(1) + " %"
                        color: "#10b981"
                        font.pixelSize: 22
                        font.bold: true
                    }
                    QQC2.Label {
                        text: "CPU: " + overviewPage.controller.psiCpuSome10.toFixed(2)
                              + "% | Mem: " + overviewPage.controller.psiMemSome10.toFixed(2)
                              + "% | IO: " + overviewPage.controller.psiIoFull10.toFixed(2) + "%"
                        color: "#cbd5e1"
                        font.pixelSize: 12
                    }
                }
            }

            // Pillar 3: ZRAM Compression Efficiency
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 110
                radius: 10
                color: Qt.rgba(0.11, 0.16, 0.25, 0.85)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: 4

                    QQC2.Label { text: "MEMORY & ZRAM COMPRESSION"; color: "#94a3b8"; font.pixelSize: 11; font.bold: true }
                    QQC2.Label {
                        text: overviewPage.controller.zramCompressionRatio.toFixed(2) + "× ZRAM"
                        color: "#38bdf8"
                        font.pixelSize: 22
                        font.bold: true
                    }
                    QQC2.Label {
                        text: Math.round(overviewPage.controller.zramOrigMib) + " MiB ➔ "
                              + Math.round(overviewPage.controller.zramComprMib) + " MiB  |  RAM "
                              + Math.round(overviewPage.controller.ramUsedPercent) + "%"
                        color: "#cbd5e1"
                        font.pixelSize: 12
                    }
                }
            }

            // Pillar 4: Power Envelope & Top Culprit
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 110
                radius: 10
                color: Qt.rgba(0.11, 0.16, 0.25, 0.85)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: 4

                    QQC2.Label { text: "POWER FLOW & TOP PROCESS"; color: "#94a3b8"; font.pixelSize: 11; font.bold: true }
                    QQC2.Label {
                        text: "~" + overviewPage.controller.packageWatts.toFixed(1) + " W  (" + overviewPage.controller.activePowerProfile + ")"
                        color: "#f59e0b"
                        font.pixelSize: 20
                        font.bold: true
                    }
                    QQC2.Label {
                        text: "Top: " + (overviewPage.controller.culpritName || "idle")
                              + " (" + overviewPage.controller.culpritCpuPercent.toFixed(1) + "% CPU)"
                        color: "#cbd5e1"
                        font.pixelSize: 12
                    }
                }
            }
        }

        // 2b. Secondary Hardware Strip (GPU, Cooling Fan, Battery ETA & RAPL Mode)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 54
            radius: 10
            color: Qt.rgba(0.09, 0.13, 0.21, 0.88)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.12)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.largeSpacing
                anchors.rightMargin: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing

                QQC2.Label {
                    text: overviewPage.controller.gpuAvailable
                          ? ("GPU (" + overviewPage.controller.gpuDriverName + "): "
                             + Math.round(overviewPage.controller.gpuBusyPercent) + "% @ "
                             + overviewPage.controller.gpuCurrentFreqMhz + " MHz • "
                             + overviewPage.controller.gpuTempCelsius.toFixed(0) + "°C"
                             + (overviewPage.controller.gpuVramTotalMib > 0
                                ? (" • VRAM " + Math.round(overviewPage.controller.gpuVramUsedMib) + "/" + Math.round(overviewPage.controller.gpuVramTotalMib) + " MiB")
                                : ""))
                          : "GPU: Integrated / Sysfs DRM idle"
                    color: "#38bdf8"
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                QQC2.Label {
                    text: "Cooling Fan: " + (overviewPage.controller.primaryFanRpm > 0
                          ? (overviewPage.controller.primaryFanRpm + " RPM")
                          : "0 RPM (Passive)")
                    color: "#cbd5e1"
                }

                QQC2.Label {
                    text: "Battery: " + overviewPage.controller.batteryTimeRemainingText
                          + (overviewPage.controller.batteryPresent
                             ? (" (" + Math.round(overviewPage.controller.batteryHealthPercent) + "% health)")
                             : "")
                    color: "#cbd5e1"
                }

                QQC2.Label {
                    text: overviewPage.controller.raplExactAvailable ? "RAPL: Hardware Exact" : "RAPL: Estimated"
                    color: overviewPage.controller.raplExactAvailable ? "#10b981" : "#f59e0b"
                    font.pixelSize: 12
                    font.bold: true
                }
            }
        }

        // 3. Topological Sensor Map (Grouped by Physical Hardware Domain)
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Heading {
                level: 3
                text: "Physical Hardware Topology"
                Layout.fillWidth: true
            }

            Repeater {
                model: [
                    { label: "All Domains", domainId: -1 },
                    { label: "CPU & SoC", domainId: 0 },
                    { label: "GPU", domainId: 1 },
                    { label: "NVMe Storage", domainId: 3 },
                    { label: "Wireless", domainId: 4 },
                    { label: "Chassis", domainId: 6 }
                ]
                delegate: QQC2.Button {
                    required property var modelData
                    text: modelData.label
                    checkable: true
                    checked: overviewPage.selectedDomainFilter === modelData.domainId
                    onClicked: overviewPage.selectedDomainFilter = modelData.domainId
                }
            }
        }

        Repeater {
            model: overviewPage.controller.domainSensors

            delegate: Rectangle {
                required property var modelData
                visible: overviewPage.selectedDomainFilter < 0 || modelData.domainId === overviewPage.selectedDomainFilter
                Layout.fillWidth: true
                implicitHeight: visible ? 62 : 0
                radius: 8
                color: Qt.rgba(0.09, 0.13, 0.21, 0.78)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.1)

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: Kirigami.Units.largeSpacing

                    ColumnLayout {
                        Layout.preferredWidth: 240
                        spacing: 2
                        QQC2.Label {
                            text: modelData.domainName
                            color: "#38bdf8"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        QQC2.Label {
                            text: modelData.label + "  [" + modelData.chipName + "]"
                            color: "#f8fafc"
                            font.bold: true
                        }
                    }

                    // Thermal Headroom Progress Bar
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            QQC2.Label {
                                text: "Temperature: " + modelData.currentCelsius.toFixed(1) + "°C / Limit: " + modelData.maxCelsius.toFixed(0) + "°C"
                                color: "#cbd5e1"
                                font.pixelSize: 12
                            }
                            Item { Layout.fillWidth: true }
                            QQC2.Label {
                                text: "Headroom: " + modelData.headroomCelsius.toFixed(1) + "°C  |  dT/dt: "
                                      + (modelData.velocityCelsiusPerSec >= 0 ? "+" : "")
                                      + modelData.velocityCelsiusPerSec.toFixed(2) + "°C/s"
                                color: modelData.headroomCelsius < 15 ? "#f97316" : "#94a3b8"
                                font.pixelSize: 12
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 8
                            radius: 4
                            color: Qt.rgba(1, 1, 1, 0.12)

                            Rectangle {
                                width: parent.width * Math.min(1.0, Math.max(0.05, modelData.currentCelsius / Math.max(1.0, modelData.maxCelsius)))
                                height: parent.height
                                radius: 4
                                color: modelData.headroomCelsius < 15 ? "#ef4444" : (modelData.headroomCelsius < 30 ? "#f59e0b" : "#10b981")
                            }
                        }
                    }
                }
            }
        }
    }
}
