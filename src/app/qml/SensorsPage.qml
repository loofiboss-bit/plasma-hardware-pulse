// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0
import "MeasurementLabels.js" as MeasurementLabels

Kirigami.ScrollablePage {
    id: sensorsPage
    objectName: "sensorsPage"
    title: KI18n.i18n("Sensors")
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None
    titleDelegate: Item {
        width: parent ? parent.width : 0
        height: 0
        visible: false
    }
    topPadding: Kirigami.Units.largeSpacing
    bottomPadding: Kirigami.Units.largeSpacing
    leftPadding: Kirigami.Units.largeSpacing
    rightPadding: Kirigami.Units.largeSpacing

    required property PulseTelemetryController controller

    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: sensorsPage.Kirigami.Theme.backgroundColor
        disabledColor: sensorsPage.Kirigami.Theme.disabledTextColor
        negativeColor: sensorsPage.Kirigami.Theme.negativeTextColor
        neutralColor: sensorsPage.Kirigami.Theme.neutralTextColor
        positiveColor: sensorsPage.Kirigami.Theme.positiveTextColor
        textColor: sensorsPage.Kirigami.Theme.textColor
        highlightColor: sensorsPage.Kirigami.Theme.highlightColor
    }

    function formatTemp(celsius) {
        return MeasurementLabels.formatTemperature(celsius, sensorsPage.controller.temperatureUnit)
    }

    ColumnLayout {
        width: sensorsPage.width - Kirigami.Units.largeSpacing * 2
        spacing: Kirigami.Units.largeSpacing

        // ==========================================
        // 1. PAGE HEADER STRIP
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            spacing: sensorsPage.theme.spacingM

            ColumnLayout {
                spacing: 2
                QQC2.Label {
                    text: KI18n.i18n("Hardware Sensors")
                    font.weight: Font.DemiBold
                    font.pointSize: sensorsPage.theme.fontHeadline
                    color: sensorsPage.theme.textColor
                }
                QQC2.Label {
                    text: KI18n.i18n("Temperature probes, fan RPM speeds, and hwmon thermal margins in real time.")
                    font.pointSize: sensorsPage.theme.fontCaption
                    color: sensorsPage.theme.mutedTextColor
                }
            }

            Item { Layout.fillWidth: true }

            // Sensor Count Badge
            Rectangle {
                radius: sensorsPage.theme.pillRadius
                color: sensorsPage.theme.cardNestedBg
                border.width: 1
                border.color: sensorsPage.theme.subtleBorderColor
                implicitHeight: 26
                implicitWidth: sensorCountRow.implicitWidth + 18

                RowLayout {
                    id: sensorCountRow
                    anchors.centerIn: parent
                    spacing: 6
                    Kirigami.Icon {
                        source: "temperature-symbolic"
                        implicitWidth: 12
                        implicitHeight: 12
                        color: sensorsPage.theme.tempAccent
                    }
                    QQC2.Label {
                        text: KI18n.i18n("%1 aktiva givare", (controller.domainSensors || []).length)
                        font.bold: true
                        font.pointSize: sensorsPage.theme.fontCaption
                        color: sensorsPage.theme.textColor
                    }
                }
            }
        }

        // ==========================================
        // 2. COOLING FANS HERO CARD
        // ==========================================
        PulseCard {
            Layout.fillWidth: true
            padding: theme.spacingL
            accentColor: theme.tempAccent
            accentStrength: 0.10

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.mediumSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        radius: 6
                        color: theme.accentTint(theme.tempAccent, theme.isDark ? 0.22 : 0.14)
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            source: "view-refresh"
                            color: theme.tempAccent
                        }
                    }
                    ColumnLayout {
                        spacing: 1
                        QQC2.Label {
                            text: KI18n.i18n("Kylfläktar & Luftflöde")
                            font.weight: Font.DemiBold
                            font.pointSize: sensorsPage.theme.fontTitle
                            color: sensorsPage.theme.textColor
                        }
                        QQC2.Label {
                            text: (controller.fans || []).length > 0 ? KI18n.i18n("%1 aktiva fläktar identifierade", controller.fans.length) : KI18n.i18n("Primärt kylsystem")
                            font.pointSize: sensorsPage.theme.fontCaption
                            color: sensorsPage.theme.mutedTextColor
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                // Multi-Fan List
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: (controller.fans || []).length > 0

                    Repeater {
                        model: controller.fans || []
                        delegate: Rectangle {
                            id: fanItem
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: fanItemLayout.implicitHeight + 16
                            radius: theme.innerRadius
                            color: theme.cardNestedBg
                            border.width: 1
                            border.color: theme.subtleBorderColor

                            ColumnLayout {
                                id: fanItemLayout
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Kirigami.Icon {
                                        source: "view-refresh"
                                        implicitWidth: 16
                                        implicitHeight: 16
                                        color: fanItem.modelData.rpm > 0 ? theme.tempAccent : theme.faintTextColor
                                        rotation: 0
                                        NumberAnimation on rotation {
                                            running: theme.motionAllowed && sensorsPage.visible && fanItem.modelData.rpm > 0
                                            from: 0
                                            to: 360
                                            loops: Animation.Infinite
                                            duration: Math.max(250, Math.min(2500, 60000 / Math.max(300, fanItem.modelData.rpm)))
                                        }
                                    }

                                    QQC2.Label {
                                        text: (fanItem.modelData.label && fanItem.modelData.label.length > 0)
                                              ? fanItem.modelData.label
                                              : (KI18n.i18n("Fläkt ") + (fanItem.modelData.name || ""))
                                        font.weight: Font.DemiBold
                                        color: sensorsPage.theme.textColor
                                        Layout.fillWidth: true
                                    }

                                    QQC2.Label {
                                        text: fanItem.modelData.rpm > 0 ? (fanItem.modelData.rpm + " RPM") : KI18n.i18n("Stoppad (0 RPM)")
                                        font.weight: Font.DemiBold
                                        color: fanItem.modelData.rpm > 0 ? sensorsPage.theme.textColor : sensorsPage.theme.mutedTextColor
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 6
                                    radius: 3
                                    color: theme.trackColor
                                    Rectangle {
                                        visible: fanItem.modelData.rpm > 0
                                        width: parent.width * Math.min(1.0, Math.max(0.0, fanItem.modelData.rpm / (fanItem.modelData.maxRpm > 0 ? fanItem.modelData.maxRpm : 5500.0)))
                                        height: parent.height
                                        radius: 3
                                        color: theme.tempAccent
                                    }
                                }
                            }
                        }
                    }
                }

                // Fallback Single Fan Display
                RowLayout {
                    Layout.fillWidth: true
                    visible: !(controller.fans || []).length
                    spacing: 8
                    Kirigami.Icon {
                        source: "view-refresh"
                        implicitWidth: 16
                        implicitHeight: 16
                        color: controller.fanAvailable && controller.primaryFanRpm > 0 ? theme.tempAccent : theme.faintTextColor
                    }
                    QQC2.Label {
                        text: controller.fanAvailable ? (controller.primaryFanRpm + " RPM") : KI18n.i18n("Fläkthastighet ej tillgänglig")
                        font.weight: Font.DemiBold
                        color: sensorsPage.theme.textColor
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // ==========================================
        // 3. COMPLETE HARDWARE SENSORS GRID
        // ==========================================
        PulseCard {
            Layout.fillWidth: true
            padding: theme.spacingL

            ColumnLayout {
                id: sensorsCol
                objectName: "section_sensors"
                focus: true
                Accessible.name: KI18n.i18n("sensors section")
                width: parent.width
                spacing: Kirigami.Units.mediumSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Kirigami.Icon {
                        source: "temperature-symbolic"
                        implicitWidth: 16
                        implicitHeight: 16
                        color: theme.tempAccent
                    }
                    Kirigami.Heading {
                        level: 3
                        text: KI18n.i18n("Identifierade hårdvarusensorer")
                        Layout.fillWidth: true
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: sensorsPage.width < 680 ? 1 : 2
                    columnSpacing: Kirigami.Units.mediumSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: controller.domainSensors || []
                        delegate: Rectangle {
                            id: sensorTile
                            required property var modelData
                            readonly property real tempRatio: sensorTile.modelData.available
                                ? Math.min(1.0, Math.max(0.0, (Number(sensorTile.modelData.currentCelsius) - 25.0) / Math.max(25.0, (Number(sensorTile.modelData.maxCelsius) || 100.0) - 25.0)))
                                : 0.0
                            Layout.fillWidth: true
                            implicitHeight: sensorTileCol.implicitHeight + 16
                            radius: theme.innerRadius
                            color: theme.cardNestedBg
                            border.width: 1
                            border.color: theme.subtleBorderColor

                            ColumnLayout {
                                id: sensorTileCol
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Rectangle {
                                        radius: 3
                                        color: theme.subtleSurfaceColor
                                        border.width: 1
                                        border.color: theme.subtleBorderColor
                                        implicitHeight: 18
                                        implicitWidth: dLbl.implicitWidth + 8
                                        QQC2.Label {
                                            id: dLbl
                                            anchors.centerIn: parent
                                            text: sensorTile.modelData.domainName || "hwmon"
                                            font.pointSize: sensorsPage.theme.fontCaption * 0.85
                                            font.weight: Font.DemiBold
                                            color: sensorsPage.theme.mutedTextColor
                                        }
                                    }

                                    QQC2.Label {
                                        Layout.fillWidth: true
                                        text: sensorTile.modelData.label || "Sensor"
                                        font.weight: Font.Medium
                                        color: sensorsPage.theme.textColor
                                        elide: Text.ElideRight
                                    }

                                    QQC2.Label {
                                        text: sensorTile.modelData.available ? sensorsPage.formatTemp(sensorTile.modelData.currentCelsius) : KI18n.i18n("Ej tillgänglig")
                                        font.weight: Font.Bold
                                        color: Number(sensorTile.modelData.currentCelsius) > 85 ? theme.negativeColor : sensorsPage.theme.textColor
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 6
                                    radius: 3
                                    color: theme.trackColor
                                    Rectangle {
                                        visible: sensorTile.modelData.available
                                        width: parent.width * sensorTile.tempRatio
                                        height: parent.height
                                        radius: 3
                                        color: Number(sensorTile.modelData.headroomCelsius) <= 5 && sensorTile.modelData.limitsAvailable
                                               ? theme.neutralColor : theme.tempAccent
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: sensorTile.modelData.limitsAvailable
                                    QQC2.Label {
                                        text: KI18n.i18n("Marginal: ") + MeasurementLabels.formatTemperatureDelta(Number(sensorTile.modelData.headroomCelsius), controller.temperatureUnit)
                                        font.pointSize: sensorsPage.theme.fontCaption * 0.9
                                        color: Number(sensorTile.modelData.headroomCelsius) <= 5 ? theme.negativeColor : sensorsPage.theme.mutedTextColor
                                    }
                                    Item { Layout.fillWidth: true }
                                    QQC2.Label {
                                        text: "Max: " + sensorsPage.formatTemp(sensorTile.modelData.maxCelsius)
                                        font.pointSize: sensorsPage.theme.fontCaption * 0.9
                                        color: sensorsPage.theme.faintTextColor
                                    }
                                }
                            }
                        }
                    }
                }

                QQC2.Label {
                    visible: (controller.domainSensors || []).length === 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.disabledTextColor
                    text: KI18n.i18n("Inga temperaturgivare med stöd hittades.")
                }
            }
        }
    }
}
