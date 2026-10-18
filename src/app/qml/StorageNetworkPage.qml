pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    objectName: "storageNetworkPage"
    title: "Storage & Network"
    required property var controller

    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: page.Kirigami.Theme.backgroundColor
        disabledColor: page.Kirigami.Theme.disabledTextColor
        negativeColor: page.Kirigami.Theme.negativeTextColor
        neutralColor: page.Kirigami.Theme.neutralTextColor
        positiveColor: page.Kirigami.Theme.positiveTextColor
    }

    function metric(value, suffix) {
        return value !== undefined && isFinite(Number(value)) ? Number(value).toFixed(2) + suffix : "Unavailable"
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        // Informational Context Banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: bannerCol.implicitHeight + Kirigami.Units.mediumSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1
            border.color: theme.cardBorderColor

            ColumnLayout {
                id: bannerCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.mediumSpacing
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Kirigami.Icon {
                        source: "drive-harddisk"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                        color: theme.storageAccent
                    }

                    Kirigami.Heading {
                        level: 3
                        text: "Devices & network interfaces"
                        Layout.fillWidth: true
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    color: Kirigami.Theme.disabledTextColor
                    text: "Live device readings. Physical and virtual devices are monitored individually. Aggregated totals use physical interfaces by default to avoid double-counting virtual tunnel or container traffic."
                }
            }
        }

        // Section: Storage Devices
        RowLayout {
            Layout.fillWidth: true
            Kirigami.Heading {
                text: "Storage devices"
                level: 2
                Layout.fillWidth: true
            }

            QQC2.RadioButton {
                text: "Auto physical totals"
                checked: page.controller.selectedStorageId.length === 0
                onClicked: page.controller.selectedStorageId = ""
                Accessible.name: "Use automatic physical device totals for storage history"
            }
        }

        QQC2.Label {
            visible: page.controller.storageDevices.length === 0
            text: "No readable storage devices found"
            color: Kirigami.Theme.disabledTextColor
        }

        GridLayout {
            Layout.fillWidth: true
            columns: page.width < 720 ? 1 : 2
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            Repeater {
                model: page.controller.storageDevices
                delegate: Rectangle {
                    id: diskCard
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: diskInnerCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                    radius: theme.cardRadius
                    color: theme.surfaceColor
                    border.width: page.controller.selectedStorageId === diskCard.modelData.id ? 1.5 : 1
                    border.color: page.controller.selectedStorageId === diskCard.modelData.id ? Kirigami.Theme.highlightColor : theme.cardBorderColor

                    ColumnLayout {
                        id: diskInnerCol
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.largeSpacing
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Kirigami.Icon {
                                source: diskCard.modelData.physical ? "drive-harddisk" : "drive-harddisk-root"
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                color: theme.storageAccent
                            }

                            QQC2.Label {
                                text: diskCard.modelData.name
                                font.bold: true
                                color: Kirigami.Theme.textColor
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                radius: theme.pillRadius
                                color: theme.subtleSurfaceColor
                                border.width: 1
                                border.color: theme.subtleBorderColor
                                implicitWidth: typeBadgeLbl.implicitWidth + 10
                                implicitHeight: typeBadgeLbl.implicitHeight + 4
                                QQC2.Label {
                                    id: typeBadgeLbl
                                    anchors.centerIn: parent
                                    text: diskCard.modelData.partition ? "Partition" : (diskCard.modelData.physical ? "Physical Drive" : "Virtual")
                                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                                    font.bold: true
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }
                        }

                        // Throughput & Latency Tiles
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 6
                            rowSpacing: 6

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: r1.implicitHeight + 10
                                radius: theme.innerRadius
                                color: theme.cardNestedBg
                                RowLayout {
                                    id: r1
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    QQC2.Label { text: "Read:"; font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: diskCard.modelData.available ? page.metric(diskCard.modelData.readMibPerSec, " MiB/s") : "—"
                                        font.bold: true
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        color: theme.storageAccent
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: r2.implicitHeight + 10
                                radius: theme.innerRadius
                                color: theme.cardNestedBg
                                RowLayout {
                                    id: r2
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    QQC2.Label { text: "Write:"; font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: diskCard.modelData.available ? page.metric(diskCard.modelData.writeMibPerSec, " MiB/s") : "—"
                                        font.bold: true
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        color: theme.cpuAccent
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: r3.implicitHeight + 10
                                radius: theme.innerRadius
                                color: theme.cardNestedBg
                                RowLayout {
                                    id: r3
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    QQC2.Label { text: "IOPS:"; font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: diskCard.modelData.available ? page.metric(diskCard.modelData.iops, "") : "—"
                                        font.bold: true
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: r4.implicitHeight + 10
                                radius: theme.innerRadius
                                color: theme.cardNestedBg
                                RowLayout {
                                    id: r4
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    QQC2.Label { text: "Avg Latency:"; font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: diskCard.modelData.requestTimeAvailable ? page.metric(diskCard.modelData.requestMs, " ms") : "—"
                                        font.bold: true
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    }
                                }
                            }
                        }

                        // Radio Selector in card footer
                        QQC2.RadioButton {
                            Layout.fillWidth: true
                            text: "Use this device for storage history"
                            checked: page.controller.selectedStorageId === diskCard.modelData.id
                            onClicked: page.controller.selectedStorageId = diskCard.modelData.id
                        }
                    }
                }
            }
        }

        // Section: Network Interfaces
        Kirigami.Heading {
            text: "Network interfaces"
            level: 2
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            color: Kirigami.Theme.disabledTextColor
            text: "Automatic totals use physical interfaces. You can change selected interfaces in Settings. Cumulative packet statistics track drops and error rates."
        }

        GridLayout {
            Layout.fillWidth: true
            columns: page.width < 720 ? 1 : 2
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            Repeater {
                model: page.controller.networkInterfaces
                delegate: Rectangle {
                    id: netCard
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: netInnerCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                    radius: theme.cardRadius
                    color: theme.surfaceColor
                    border.width: 1
                    border.color: theme.cardBorderColor

                    ColumnLayout {
                        id: netInnerCol
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.largeSpacing
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Kirigami.Icon {
                                source: netCard.modelData.physical ? "network-wired" : "network-wireless"
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                color: theme.psiAccent
                            }

                            QQC2.Label {
                                text: netCard.modelData.name
                                font.bold: true
                                color: Kirigami.Theme.textColor
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                radius: theme.pillRadius
                                color: netCard.modelData.included
                                       ? theme.accentTint(Kirigami.Theme.positiveTextColor, 0.16)
                                       : theme.subtleSurfaceColor
                                border.width: 1
                                border.color: netCard.modelData.included ? Kirigami.Theme.positiveTextColor : theme.subtleBorderColor
                                implicitWidth: incBadgeLbl.implicitWidth + 10
                                implicitHeight: incBadgeLbl.implicitHeight + 4
                                QQC2.Label {
                                    id: incBadgeLbl
                                    anchors.centerIn: parent
                                    text: netCard.modelData.included ? "Included" : (netCard.modelData.physical ? "Physical" : "Virtual")
                                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                                    font.bold: true
                                    color: netCard.modelData.included ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                                }
                            }
                        }

                        // Throughput readout
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: rxRow.implicitHeight + 10
                                radius: theme.innerRadius
                                color: theme.cardNestedBg
                                RowLayout {
                                    id: rxRow
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    QQC2.Label { text: "↓ Receive:"; font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: netCard.modelData.available ? page.metric(netCard.modelData.rxMibPerSec, " MiB/s") : "—"
                                        font.bold: true
                                        color: theme.psiAccent
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: txRow.implicitHeight + 10
                                radius: theme.innerRadius
                                color: theme.cardNestedBg
                                RowLayout {
                                    id: txRow
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    QQC2.Label { text: "↑ Send:"; font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: netCard.modelData.available ? page.metric(netCard.modelData.txMibPerSec, " MiB/s") : "—"
                                        font.bold: true
                                        color: theme.cpuAccent
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    }
                                }
                            }
                        }

                        // Drops and Errors
                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                            color: Kirigami.Theme.disabledTextColor
                            text: netCard.modelData.countersAvailable
                                  ? "Errors: ↓ " + page.metric(netCard.modelData.rxErrors, "") + " · ↑ " + page.metric(netCard.modelData.txErrors, "")
                                    + "   |   Drops: ↓ " + page.metric(netCard.modelData.rxDropped, "") + " · ↑ " + page.metric(netCard.modelData.txDropped, "")
                                  : "Error and drop counters unavailable for this interface."
                        }
                    }
                }
            }
        }

        // Section: Measurement Capabilities
        Kirigami.Heading {
            text: "Measurement capabilities"
            level: 2
        }

        GridLayout {
            Layout.fillWidth: true
            columns: page.width < 720 ? 1 : 2
            columnSpacing: Kirigami.Units.smallSpacing
            rowSpacing: Kirigami.Units.smallSpacing

            Repeater {
                model: page.controller.capabilities
                delegate: Rectangle {
                    id: capCard
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: capCol.implicitHeight + 16
                    radius: theme.innerRadius
                    color: theme.surfaceColor
                    border.width: 1
                    border.color: theme.cardBorderColor

                    ColumnLayout {
                        id: capCol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 3

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Rectangle {
                                Layout.preferredWidth: 8
                                Layout.preferredHeight: 8
                                radius: 4
                                color: capCard.modelData.available ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                            }

                            QQC2.Label {
                                text: capCard.modelData.name
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                radius: theme.pillRadius
                                color: capCard.modelData.available
                                       ? theme.accentTint(Kirigami.Theme.positiveTextColor, 0.14)
                                       : theme.subtleSurfaceColor
                                border.width: 1
                                border.color: capCard.modelData.available ? Kirigami.Theme.positiveTextColor : theme.subtleBorderColor
                                implicitWidth: capStatusLbl.implicitWidth + 10
                                implicitHeight: capStatusLbl.implicitHeight + 4
                                QQC2.Label {
                                    id: capStatusLbl
                                    anchors.centerIn: parent
                                    text: capCard.modelData.available ? "Available" : "Unavailable"
                                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                                    font.bold: true
                                    color: capCard.modelData.available ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                                }
                            }
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                            text: capCard.modelData.detail
                        }
                    }
                }
            }
        }
    }
}
