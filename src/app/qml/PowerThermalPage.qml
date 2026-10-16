import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: powerPage
    title: "Energy Flow, Power Control & CPU Core Matrix"

    required property var controller

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        // 1. Power Flow & D-Bus Profile Switcher
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: pwrCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: 12
            color: Qt.rgba(0.09, 0.13, 0.21, 0.92)
            border.width: 1
            border.color: "#f59e0b"

            ColumnLayout {
                id: pwrCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                RowLayout {
                    Layout.fillWidth: true
                    Kirigami.Heading {
                        level: 3
                        text: "Energy Flow & Thermal Power Envelope (RAPL PL1 / PL2)"
                        color: "#f8fafc"
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: powerPage.controller.raplExactAvailable ? "RAPL: Hardware Counter" : "RAPL: Estimated (see udev rule)"
                        color: powerPage.controller.raplExactAvailable ? "#10b981" : "#38bdf8"
                        font.bold: true
                    }
                    Item { Layout.preferredWidth: 12 }
                    QQC2.Label {
                        text: powerPage.controller.onAcPower ? "Power Source: AC Adapter" : "Power Source: Battery"
                        color: "#10b981"
                        font.bold: true
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 8

                    QQC2.Label {
                        text: "Package Power: ~" + powerPage.controller.packageWatts.toFixed(1) + " W"
                        color: "#f59e0b"
                        font.bold: true
                        font.pixelSize: 16
                    }
                    QQC2.Label {
                        text: "PL1 (Sustained): " + Math.round(powerPage.controller.pl1LimitWatts)
                              + " W  |  PL2 (Burst): " + Math.round(powerPage.controller.pl2LimitWatts) + " W"
                        color: "#cbd5e1"
                    }
                    QQC2.Label {
                        text: "Cooling Fan: " + (powerPage.controller.primaryFanRpm > 0
                              ? (powerPage.controller.primaryFanRpm + " RPM")
                              : "0 RPM (Passive)")
                        color: "#cbd5e1"
                    }
                    QQC2.Label {
                        text: "Battery: " + Math.round(powerPage.controller.batteryPercent)
                              + "% • " + powerPage.controller.batteryTimeRemainingText
                              + (powerPage.controller.batteryPresent
                                 ? (" (" + Math.round(powerPage.controller.batteryHealthPercent) + "% health)")
                                 : "")
                        color: "#cbd5e1"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.mediumSpacing

                    QQC2.Label {
                        text: "Active Power Profile (D-Bus):"
                        color: "#cbd5e1"
                        font.bold: true
                    }

                    Repeater {
                        model: ["power-saver", "balanced", "performance"]
                        delegate: QQC2.Button {
                            required property string modelData
                            text: modelData
                            checkable: true
                            checked: powerPage.controller.activePowerProfile === modelData
                            onClicked: powerPage.controller.setPowerProfile(modelData)
                        }
                    }

                    Item { Layout.fillWidth: true }

                    QQC2.Label {
                        text: "Total Throttle Events since boot: " + powerPage.controller.totalThrottleEvents
                              + " (Δ/s: +" + powerPage.controller.deltaThrottleEvents + ")"
                        color: powerPage.controller.deltaThrottleEvents > 0 ? "#ef4444" : "#94a3b8"
                        font.bold: true
                    }
                }
            }
        }

        // 2. Per-Core Frequency, Load & Thermal Matrix
        Kirigami.Heading {
            level: 3
            text: "Logical CPU Threads / Cores (Clock Frequency, Load & Throttling)"
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            Repeater {
                model: powerPage.controller.coreItems

                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 74
                    radius: 8
                    color: Qt.rgba(0.10, 0.14, 0.23, 0.85)
                    border.width: 1
                    border.color: modelData.throttleCount > 0 ? "#f97316" : Qt.rgba(1, 1, 1, 0.12)

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.mediumSpacing
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            QQC2.Label {
                                text: "CPU #" + modelData.coreId
                                color: "#38bdf8"
                                font.bold: true
                            }
                            QQC2.Label {
                                text: Math.round(modelData.loadPercent) + "% load"
                                color: "#f8fafc"
                                font.bold: true
                            }
                            QQC2.Label {
                                visible: modelData.throttleCount > 0
                                text: "⚠ " + modelData.throttleCount + " thr"
                                color: "#f97316"
                                font.pixelSize: 11
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            QQC2.Label {
                                text: modelData.currentFreqMhz + " / " + modelData.maxFreqMhz + " MHz"
                                color: "#cbd5e1"
                                font.family: "monospace"
                            }
                            QQC2.Label {
                                text: Math.round(modelData.tempCelsius) + "°C"
                                color: modelData.tempCelsius >= 85 ? "#ef4444" : "#10b981"
                                font.bold: true
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 7
                            radius: 3.5
                            color: Qt.rgba(1, 1, 1, 0.12)

                            Rectangle {
                                width: parent.width * Math.min(1.0, Math.max(0.02, modelData.loadPercent / 100.0))
                                height: parent.height
                                radius: 3.5
                                color: modelData.loadPercent > 80 ? "#f59e0b" : "#06b6d4"
                            }
                        }
                    }
                }
            }
        }
    }
}
