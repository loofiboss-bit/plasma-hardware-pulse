pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: powerPage
    objectName: "powerPage"
    title: "Power & Cooling"
    required property var controller

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Card {
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.mediumSpacing

                Kirigami.Heading {
                    text: "Power and cooling"
                    level: 2
                    Layout.fillWidth: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: width < 620 ? 1 : 2
                    columnSpacing: Kirigami.Units.largeSpacing
                    rowSpacing: Kirigami.Units.mediumSpacing

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: powerPage.controller.packagePowerAvailable
                              ? "CPU package power: " + powerPage.controller.packageWatts.toFixed(1) + " W · "
                                + (powerPage.controller.packagePowerEstimated ? "Estimated" : "Measured")
                              : "CPU package power: Unavailable"
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: powerPage.controller.powerLimitsAvailable
                              ? "Power limits: PL1 " + Math.round(powerPage.controller.pl1LimitWatts) + " W · PL2 "
                                + Math.round(powerPage.controller.pl2LimitWatts) + " W"
                              : "Power limits: Unavailable"
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Fan speed: " + (powerPage.controller.fanAvailable
                              ? powerPage.controller.primaryFanRpm + " RPM" : "Unavailable")
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Power source: " + (powerPage.controller.batteryStateAvailable
                              ? (powerPage.controller.onAcPower ? "AC power" : "Battery") : "Unavailable")
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: !powerPage.controller.batteryStateAvailable ? "Battery: Unavailable"
                              : (!powerPage.controller.batteryPresent ? "Battery: Not present · " + powerPage.controller.batteryTimeRemainingText
                                 : "Battery: " + (powerPage.controller.batteryLevelAvailable
                                   ? Math.round(powerPage.controller.batteryPercent) + "%" : "Level unavailable")
                                   + " · " + powerPage.controller.batteryTimeRemainingText)
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: powerPage.controller.batteryPresent
                              ? "Battery health: " + (powerPage.controller.batteryHealthAvailable
                                ? Math.round(powerPage.controller.batteryHealthPercent) + "%" : "Unavailable")
                              : "Battery health: Unavailable"
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "NVMe temperature: " + (powerPage.controller.nvmeTempAvailable
                              ? Math.round(powerPage.controller.nvmeTempCelsius) + " °C" : "Unavailable")
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Wireless temperature: " + (powerPage.controller.wifiTempAvailable
                              ? Math.round(powerPage.controller.wifiTempCelsius) + " °C" : "Unavailable")
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "GPU load: " + (powerPage.controller.gpuBusyAvailable
                              ? Math.round(powerPage.controller.gpuBusyPercent) + "%" : "Unavailable")
                              + " · GPU temperature: " + (powerPage.controller.gpuTempAvailable
                              ? Math.round(powerPage.controller.gpuTempCelsius) + " °C" : "Unavailable")
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: powerPage.controller.throttleCountersAvailable
                              ? "Throttle events since boot: " + powerPage.controller.totalThrottleEvents
                                + " · New this interval: " + powerPage.controller.deltaThrottleEvents
                              : "Throttle event counters: Unavailable"
                    }
                }
            }
        }

        QQC2.GroupBox {
            Layout.fillWidth: true
            title: "Power profile"

            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: !powerPage.controller.powerProfileServiceAvailable
                          ? "Power profile service unavailable"
                          : "Active profile: " + (powerPage.controller.activePowerProfile || "Unavailable")
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: powerPage.controller.availablePowerProfiles
                        delegate: QQC2.Button {
                            required property string modelData
                            text: powerPage.controller.powerProfileChangePending ? "Applying…" : modelData
                            checkable: true
                            checked: powerPage.controller.activePowerProfile === modelData
                            enabled: powerPage.controller.powerProfileServiceAvailable
                                     && !powerPage.controller.powerProfileChangePending
                            Accessible.name: modelData + (checked ? ", active profile" : ", change power profile")
                            onClicked: powerPage.controller.setPowerProfile(modelData)
                        }
                    }
                }
            }
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            level: 2
            text: "CPU cores and sensors"
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width < 560 ? 1 : 2
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            Repeater {
                model: powerPage.controller.coreItems
                delegate: Kirigami.Card {
                    id: coreCard
                    required property var modelData
                    Layout.fillWidth: true
                    contentItem: ColumnLayout {
                        QQC2.Label {
                            text: "CPU " + coreCard.modelData.coreId
                            font.bold: true
                            color: Kirigami.Theme.textColor
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Load: " + (coreCard.modelData.loadAvailable
                                  ? Math.round(coreCard.modelData.loadPercent) + "%" : "Unavailable")
                                  + " · Frequency: " + (coreCard.modelData.currentFreqMhz > 0
                                  ? coreCard.modelData.currentFreqMhz + " MHz" : "Unavailable")
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Throttle count: " + (coreCard.modelData.throttleCountersAvailable
                                  ? coreCard.modelData.throttleCount : "Unavailable")
                        }
                    }
                }
            }
        }

        QQC2.GroupBox {
            Layout.fillWidth: true
            title: "Detected sensors"

            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: powerPage.controller.domainSensors
                    delegate: QQC2.Label {
                        required property var modelData
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: modelData.domainName + " · " + modelData.label + ": "
                              + (modelData.available ? Number(modelData.currentCelsius).toFixed(1) + " °C" : "Unavailable")
                        Accessible.name: text
                    }
                }
                QQC2.Label {
                    visible: powerPage.controller.domainSensors.length === 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "No supported temperature sensors were found."
                }
            }
        }
    }
}
