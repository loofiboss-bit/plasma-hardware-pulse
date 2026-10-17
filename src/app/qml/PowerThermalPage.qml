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
    property bool sortCoresByLoad: false

    function orderedCoreItems() {
        var list = powerPage.controller.coreItems ? powerPage.controller.coreItems.slice() : []
        if (powerPage.sortCoresByLoad) {
            list.sort(function(a, b) {
                var diff = (Number(b.loadPercent) || 0) - (Number(a.loadPercent) || 0)
                return Math.abs(diff) > 0.1 ? diff : ((Number(a.coreId) || 0) - (Number(b.coreId) || 0))
            })
        }
        return list
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        GridLayout {
            Layout.fillWidth: true
            columns: width < 680 ? 1 : 2
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            // 1. CPU Package & Power Limits Card
            Kirigami.Card {
                Layout.fillWidth: true
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing
                    QQC2.Label {
                        text: "CPU package & power limits"
                        color: Kirigami.Theme.disabledTextColor
                        font.bold: true
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Kirigami.Heading {
                        level: 3
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: powerPage.controller.packagePowerAvailable
                              ? powerPage.controller.packageWatts.toFixed(1) + " W · "
                                + (powerPage.controller.packagePowerEstimated ? "Estimated" : "Measured")
                              : "CPU package power: Unavailable"
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 6
                        radius: 3
                        color: Kirigami.Theme.backgroundColor
                        Rectangle {
                            visible: powerPage.controller.packagePowerAvailable
                            width: parent.width * Math.min(1.0, Math.max(0.0, powerPage.controller.packageWatts / Math.max(25.0, powerPage.controller.pl1LimitWatts > 0 ? powerPage.controller.pl1LimitWatts : 45.0)))
                            height: parent.height
                            radius: 3
                            color: Kirigami.Theme.highlightColor
                        }
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
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: powerPage.controller.throttleCountersAvailable
                              ? "Throttle events since boot: " + powerPage.controller.totalThrottleEvents
                                + " · New this interval: " + powerPage.controller.deltaThrottleEvents
                              : "Throttle event counters: Unavailable"
                    }
                }
            }

            // 2. Graphics (GPU) Card
            Kirigami.Card {
                Layout.fillWidth: true
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing
                    QQC2.Label {
                        text: "Graphics (GPU)"
                        color: Kirigami.Theme.disabledTextColor
                        font.bold: true
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Kirigami.Heading {
                        level: 3
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "GPU load: " + (powerPage.controller.gpuBusyAvailable
                              ? Math.round(powerPage.controller.gpuBusyPercent) + "%" : "Unavailable")
                              + " · GPU temperature: " + (powerPage.controller.gpuTempAvailable
                              ? Math.round(powerPage.controller.gpuTempCelsius) + " °C" : "Unavailable")
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 6
                        radius: 3
                        color: Kirigami.Theme.backgroundColor
                        Rectangle {
                            visible: powerPage.controller.gpuBusyAvailable
                            width: parent.width * Math.min(1.0, Math.max(0.0, powerPage.controller.gpuBusyPercent / 100.0))
                            height: parent.height
                            radius: 3
                            color: powerPage.controller.gpuBusyPercent > 85 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.highlightColor
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Driver: " + (powerPage.controller.gpuAvailable && powerPage.controller.gpuDriverName
                              ? powerPage.controller.gpuDriverName : "Unavailable")
                              + " · Frequency: " + (powerPage.controller.gpuFrequencyAvailable
                              ? powerPage.controller.gpuCurrentFreqMhz
                                + (powerPage.controller.gpuBoostFreqMhz > 0 ? " / " + powerPage.controller.gpuBoostFreqMhz : "") + " MHz"
                              : "Unavailable")
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: "VRAM: " + (powerPage.controller.gpuVramAvailable
                              ? Math.round(powerPage.controller.gpuVramUsedMib) + " / " + Math.round(powerPage.controller.gpuVramTotalMib) + " MiB"
                              : "Unavailable")
                              + " · GPU power: " + (powerPage.controller.gpuPowerAvailable
                              ? powerPage.controller.gpuPowerWatts.toFixed(1) + " W" : "Unavailable")
                    }
                }
            }

            // 3. Battery & Power Source Card
            Kirigami.Card {
                Layout.fillWidth: true
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing
                    QQC2.Label {
                        text: "Battery & power source"
                        color: Kirigami.Theme.disabledTextColor
                        font.bold: true
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Kirigami.Heading {
                        level: 3
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Power source: " + (powerPage.controller.batteryStateAvailable
                              ? (powerPage.controller.onAcPower ? "AC power" : "Battery") : "Unavailable")
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 6
                        radius: 3
                        color: Kirigami.Theme.backgroundColor
                        Rectangle {
                            visible: powerPage.controller.batteryPresent && powerPage.controller.batteryLevelAvailable
                            width: parent.width * Math.min(1.0, Math.max(0.0, powerPage.controller.batteryPercent / 100.0))
                            height: parent.height
                            radius: 3
                            color: powerPage.controller.batteryPercent < 20 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.positiveTextColor
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: !powerPage.controller.batteryStateAvailable ? "Battery: Unavailable"
                              : (!powerPage.controller.batteryPresent ? "Battery: Not present · " + powerPage.controller.batteryTimeRemainingText
                                 : "Battery: " + (powerPage.controller.batteryLevelAvailable
                                   ? Math.round(powerPage.controller.batteryPercent) + "%" : "Level unavailable")
                                   + " · " + powerPage.controller.batteryTimeRemainingText
                                   + (powerPage.controller.batteryPowerAvailable ? " · " + powerPage.controller.batteryWatts.toFixed(1) + " W" : ""))
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: powerPage.controller.batteryPresent
                              ? "Battery health: " + (powerPage.controller.batteryHealthAvailable
                                ? Math.round(powerPage.controller.batteryHealthPercent) + "%" : "Unavailable")
                              : "Battery health: Unavailable"
                    }
                }
            }

            // 4. Cooling & Device Thermal Card
            Kirigami.Card {
                Layout.fillWidth: true
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing
                    QQC2.Label {
                        text: "Cooling & device thermal"
                        color: Kirigami.Theme.disabledTextColor
                        font.bold: true
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Kirigami.Heading {
                        level: 3
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Fan speed: " + (powerPage.controller.fanAvailable
                              ? powerPage.controller.primaryFanRpm + " RPM" : "Unavailable")
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
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: "Wireless temperature: " + (powerPage.controller.wifiTempAvailable
                              ? Math.round(powerPage.controller.wifiTempCelsius) + " °C" : "Unavailable")
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

        RowLayout {
            Layout.fillWidth: true
            Kirigami.Heading {
                Layout.fillWidth: true
                level: 2
                text: "CPU cores and sensors"
                wrapMode: Text.WordWrap
            }
            QQC2.Button {
                text: powerPage.sortCoresByLoad ? "Sorted by load" : "Sort by load"
                icon.name: "view-sort-descending"
                checkable: true
                checked: powerPage.sortCoresByLoad
                onClicked: powerPage.sortCoresByLoad = checked
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width < 560 ? 1 : (width < 900 ? 2 : 4)
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            Repeater {
                model: powerPage.orderedCoreItems()
                delegate: Kirigami.Card {
                    id: coreCard
                    required property var modelData
                    Layout.fillWidth: true
                    contentItem: ColumnLayout {
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            QQC2.Label {
                                text: "CPU " + coreCard.modelData.coreId
                                font.bold: true
                                color: Kirigami.Theme.textColor
                                Layout.fillWidth: true
                            }
                            QQC2.Label {
                                visible: coreCard.modelData.tempCelsius > 0
                                text: Math.round(coreCard.modelData.tempCelsius) + " °C"
                                color: Kirigami.Theme.disabledTextColor
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            radius: 3
                            color: Kirigami.Theme.backgroundColor
                            Rectangle {
                                visible: coreCard.modelData.loadAvailable
                                width: parent.width * Math.min(1.0, Math.max(0.0, coreCard.modelData.loadPercent / 100.0))
                                height: parent.height
                                radius: 3
                                color: coreCard.modelData.loadPercent > 85 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.highlightColor
                            }
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Load: " + (coreCard.modelData.loadAvailable
                                  ? Math.round(coreCard.modelData.loadPercent) + "%" : "Unavailable")
                                  + " · " + (coreCard.modelData.currentFreqMhz > 0
                                  ? coreCard.modelData.currentFreqMhz + " MHz" : "Unavailable")
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
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
