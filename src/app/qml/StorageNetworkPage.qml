pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    objectName: "storageNetworkPage"
    title: KI18n.i18n("Storage & Network")
    titleDelegate: Item {
        width: parent ? parent.width : 0
        height: 0
        visible: false
    }
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
        return value !== undefined && isFinite(Number(value)) ? Number(value).toFixed(2) + suffix : KI18n.i18n("Unavailable")
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
                        text: KI18n.i18n("Devices & network interfaces")
                        Layout.fillWidth: true
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    color: Kirigami.Theme.disabledTextColor
                    text: KI18n.i18n("Live device readings. Physical and virtual devices are monitored individually. Aggregated totals use physical interfaces by default to avoid double-counting virtual tunnel or container traffic.")
                }
            }
        }

        // Section: Storage Devices
        RowLayout {
            Layout.fillWidth: true
            Kirigami.Heading {
                text: KI18n.i18n("Storage devices")
                level: 2
                Layout.fillWidth: true
            }

            QQC2.RadioButton {
                text: KI18n.i18n("Auto physical totals")
                checked: page.controller.selectedStorageId.length === 0
                onClicked: page.controller.selectedStorageId = ""
                Accessible.name: KI18n.i18n("Use automatic physical device totals for storage history")
            }
        }

        QQC2.Label {
            visible: page.controller.storageDevices.length === 0
            text: KI18n.i18n("No readable storage devices found")
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
                                    text: diskCard.modelData.partition ? "Partition" : (diskCard.modelData.physical ? KI18n.i18n("Physical Drive") : "Virtual")
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
                                    QQC2.Label { text: KI18n.i18n("Read:"); font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: diskCard.modelData.available ? page.metric(diskCard.modelData.readMibPerSec, KI18n.i18n(" MiB/s")) : "—"
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
                                    QQC2.Label { text: KI18n.i18n("Write:"); font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: diskCard.modelData.available ? page.metric(diskCard.modelData.writeMibPerSec, KI18n.i18n(" MiB/s")) : "—"
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
                                    QQC2.Label { text: KI18n.i18n("IOPS:"); font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
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
                                    QQC2.Label { text: KI18n.i18n("Avg Latency:"); font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: diskCard.modelData.requestTimeAvailable ? page.metric(diskCard.modelData.requestMs, KI18n.i18n(" ms")) : "—"
                                        font.bold: true
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    }
                                }
                            }
                        }

                        // Radio Selector in card footer
                        QQC2.RadioButton {
                            Layout.fillWidth: true
                            text: KI18n.i18n("Use this device for storage history")
                            checked: page.controller.selectedStorageId === diskCard.modelData.id
                            onClicked: page.controller.selectedStorageId = diskCard.modelData.id
                        }
                    }
                }
            }
        }

        // Section: NVMe Controllers & Health
        RowLayout {
            Layout.fillWidth: true
            visible: (page.controller.nvmeControllers || []).length > 0

            Kirigami.Heading {
                text: KI18n.i18n("NVMe controllers & hardware health")
                level: 2
                Layout.fillWidth: true
            }

            Rectangle {
                visible: page.controller.nvmeWarningActive
                radius: theme.pillRadius
                color: theme.accentTint(Kirigami.Theme.negativeTextColor, 0.2)
                border.width: 1
                border.color: Kirigami.Theme.negativeTextColor
                implicitWidth: nvmeWarnLbl.implicitWidth + 12
                implicitHeight: nvmeWarnLbl.implicitHeight + 6

                QQC2.Label {
                    id: nvmeWarnLbl
                    anchors.centerIn: parent
                    text: KI18n.i18n("CRITICAL HARDWARE WARNING")
                    font.bold: true
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize - 1
                    color: Kirigami.Theme.negativeTextColor
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            visible: (page.controller.nvmeControllers || []).length > 0
            columns: page.width < 720 ? 1 : 2
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            Repeater {
                model: page.controller.nvmeControllers || []
                delegate: Rectangle {
                    id: nvmeCard
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: nvmeInnerCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                    radius: theme.cardRadius
                    color: theme.surfaceColor
                    border.width: 1
                    border.color: nvmeCard.modelData.criticalWarningAlarm ? Kirigami.Theme.negativeTextColor : theme.cardBorderColor

                    ColumnLayout {
                        id: nvmeInnerCol
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.largeSpacing
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Kirigami.Icon {
                                source: nvmeCard.modelData.criticalWarningAlarm ? "dialog-warning" : "drive-harddisk-solidstate"
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                color: nvmeCard.modelData.criticalWarningAlarm ? Kirigami.Theme.negativeTextColor : theme.storageAccent
                            }

                            QQC2.Label {
                                text: (nvmeCard.modelData.id || "") + (nvmeCard.modelData.model ? " · " + nvmeCard.modelData.model : "")
                                font.bold: true
                                color: Kirigami.Theme.textColor
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }

                            Rectangle {
                                radius: theme.pillRadius
                                color: nvmeCard.modelData.criticalWarningAlarm
                                       ? theme.accentTint(Kirigami.Theme.negativeTextColor, 0.18)
                                       : theme.accentTint(Kirigami.Theme.positiveTextColor, 0.14)
                                border.width: 1
                                border.color: nvmeCard.modelData.criticalWarningAlarm ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.positiveTextColor
                                implicitWidth: nvmeHealthLbl.implicitWidth + 10
                                implicitHeight: nvmeHealthLbl.implicitHeight + 4

                                QQC2.Label {
                                    id: nvmeHealthLbl
                                    anchors.centerIn: parent
                                    text: nvmeCard.modelData.criticalWarningAlarm ? KI18n.i18n("WARNING (0x%1)").arg(Number(nvmeCard.modelData.criticalWarning).toString(16)) : KI18n.i18n("Healthy (0x0)")
                                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                                    font.bold: true
                                    color: nvmeCard.modelData.criticalWarningAlarm ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.positiveTextColor
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: tempRow.implicitHeight + 10
                                radius: theme.innerRadius
                                color: theme.cardNestedBg
                                RowLayout {
                                    id: tempRow
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    QQC2.Label { text: KI18n.i18n("Composite temp:"); font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: nvmeCard.modelData.compositeTempAvailable ? (Number(nvmeCard.modelData.compositeTempCelsius).toFixed(1) + " °C") : KI18n.i18n("Unavailable")
                                        font.bold: true
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        color: nvmeCard.modelData.compositeTempCelsius > 70.0 ? Kirigami.Theme.negativeTextColor : theme.tempAccent
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: stateRow.implicitHeight + 10
                                radius: theme.innerRadius
                                color: theme.cardNestedBg
                                RowLayout {
                                    id: stateRow
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    QQC2.Label { text: KI18n.i18n("Controller state:"); font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: nvmeCard.modelData.state ? nvmeCard.modelData.state : KI18n.i18n("Unknown")
                                        font.bold: true
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Section: Network Interfaces
        Kirigami.Heading {
            text: KI18n.i18n("Network interfaces")
            level: 2
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            color: Kirigami.Theme.disabledTextColor
            text: KI18n.i18n("Automatic totals use physical interfaces. You can change selected interfaces in Settings. Cumulative packet statistics track drops and error rates.")
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
                                    QQC2.Label { text: KI18n.i18n("↓ Receive:"); font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: netCard.modelData.available ? page.metric(netCard.modelData.rxMibPerSec, KI18n.i18n(" MiB/s")) : "—"
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
                                    QQC2.Label { text: KI18n.i18n("↑ Send:"); font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                                    QQC2.Label {
                                        text: netCard.modelData.available ? page.metric(netCard.modelData.txMibPerSec, KI18n.i18n(" MiB/s")) : "—"
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
                                  ? KI18n.i18n("Errors: ↓ ") + page.metric(netCard.modelData.rxErrors, "") + " · ↑ " + page.metric(netCard.modelData.txErrors, "")
                                    + KI18n.i18n("   |   Drops: ↓ ") + page.metric(netCard.modelData.rxDropped, "") + " · ↑ " + page.metric(netCard.modelData.txDropped, "")
                                  : KI18n.i18n("Error and drop counters unavailable for this interface.")
                        }
                    }
                }
            }
        }

        // Section: Measurement Capabilities
        Kirigami.Heading {
            text: KI18n.i18n("Measurement capabilities")
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
                                    text: capCard.modelData.available ? KI18n.i18n("Available") : KI18n.i18n("Unavailable")
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
