// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

Kirigami.ScrollablePage {
    id: batteryPage
    objectName: "batteryPage"
    title: KI18n.i18nd("loofi-pulse", "Battery")
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
        bgColor: batteryPage.Kirigami.Theme.backgroundColor
        disabledColor: batteryPage.Kirigami.Theme.disabledTextColor
        negativeColor: batteryPage.Kirigami.Theme.negativeTextColor
        neutralColor: batteryPage.Kirigami.Theme.neutralTextColor
        positiveColor: batteryPage.Kirigami.Theme.positiveTextColor
        textColor: batteryPage.Kirigami.Theme.textColor
        highlightColor: batteryPage.Kirigami.Theme.highlightColor
    }

    readonly property bool hasBattery: controller.batteryPresent

    ColumnLayout {
        width: batteryPage.width - Kirigami.Units.largeSpacing * 2
        spacing: Kirigami.Units.largeSpacing

        // ==========================================
        // 1. PAGE HEADER STRIP
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            spacing: batteryPage.theme.spacingM

            ColumnLayout {
                spacing: 2
                QQC2.Label {
                    text: KI18n.i18nd("loofi-pulse", "Battery & Power")
                    font.weight: Font.DemiBold
                    font.pointSize: batteryPage.theme.fontHeadline
                    color: batteryPage.theme.textColor
                }
                QQC2.Label {
                    text: KI18n.i18nd("loofi-pulse", "Power source, battery charge level, discharge rate, health and peripherals.")
                    font.pointSize: batteryPage.theme.fontCaption
                    color: batteryPage.theme.mutedTextColor
                }
            }

            Item { Layout.fillWidth: true }

            // Power Source Badge
            Rectangle {
                radius: batteryPage.theme.pillRadius
                color: batteryPage.theme.cardNestedBg
                border.width: 1
                border.color: batteryPage.theme.subtleBorderColor
                implicitHeight: 26
                implicitWidth: pwrSourceRow.implicitWidth + 18

                RowLayout {
                    id: pwrSourceRow
                    anchors.centerIn: parent
                    spacing: 6
                    Kirigami.Icon {
                        source: controller.onAcPower ? "ac-adapter" : "battery-charging"
                        implicitWidth: 12
                        implicitHeight: 12
                        color: controller.onAcPower ? batteryPage.theme.positiveColor : batteryPage.theme.batteryAccent
                    }
                    QQC2.Label {
                        text: controller.onAcPower ? KI18n.i18n("AC Power Connected") : KI18n.i18n("On Battery Power")
                        font.bold: true
                        font.pointSize: batteryPage.theme.fontCaption
                        color: controller.onAcPower ? batteryPage.theme.positiveColor : batteryPage.theme.batteryAccent
                    }
                }
            }
        }

        // ==========================================
        // 2. PRIMARY BATTERY HERO CARD
        // ==========================================
        PulseCard {
            Layout.fillWidth: true
            padding: theme.spacingL
            accentColor: theme.batteryAccent
            accentStrength: 0.12

            ColumnLayout {
                id: heroCol
                objectName: "section_battery"
                focus: true
                Accessible.name: KI18n.i18n("battery section")
                width: parent.width
                spacing: Kirigami.Units.mediumSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        radius: 6
                        color: theme.accentTint(theme.batteryAccent, theme.isDark ? 0.22 : 0.14)
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            source: "battery-profile-performance"
                            color: theme.batteryAccent
                        }
                    }
                    ColumnLayout {
                        spacing: 1
                        QQC2.Label {
                            text: batteryPage.hasBattery ? KI18n.i18n("Primary System Battery") : KI18n.i18n("Power Delivery")
                            font.weight: Font.DemiBold
                            font.pointSize: batteryPage.theme.fontTitle
                            color: batteryPage.theme.textColor
                        }
                        QQC2.Label {
                            text: controller.batteryStateAvailable
                                  ? (controller.onAcPower ? KI18n.i18n("Running on AC adapter") : KI18n.i18n("Discharging on battery power"))
                                  : KI18n.i18n("Power status unavailable")
                            font.pointSize: batteryPage.theme.fontCaption
                            color: batteryPage.theme.mutedTextColor
                        }
                    }
                    Item { Layout.fillWidth: true }
                    QQC2.Label {
                        visible: batteryPage.hasBattery && controller.batteryLevelAvailable
                        text: Math.round(controller.batteryPercent) + "%"
                        font.weight: Font.Bold
                        font.pointSize: batteryPage.theme.fontHeadline * 1.3
                        color: controller.batteryPercent < 20 ? theme.negativeColor : theme.batteryAccent
                    }
                }

                // Charge Level Bar
                Rectangle {
                    visible: batteryPage.hasBattery
                    Layout.fillWidth: true
                    Layout.preferredHeight: 10
                    radius: 5
                    color: theme.trackColor
                    Rectangle {
                        visible: controller.batteryLevelAvailable
                        width: parent.width * Math.min(1.0, Math.max(0.0, controller.batteryPercent / 100.0))
                        height: parent.height
                        radius: 5
                        color: controller.batteryPercent < 20 ? theme.negativeColor : theme.batteryAccent
                    }
                }

                // Key Metric Grid
                GridLayout {
                    Layout.fillWidth: true
                    columns: batteryPage.width < 680 ? 2 : 4
                    columnSpacing: Kirigami.Units.mediumSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    // Status / ETA
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: etaCol.implicitHeight + 14
                        radius: theme.innerRadius
                        color: theme.cardNestedBg
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        ColumnLayout {
                            id: etaCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            QQC2.Label {
                                text: KI18n.i18n("Time Remaining")
                                font.pointSize: batteryPage.theme.fontCaption
                                color: batteryPage.theme.mutedTextColor
                            }
                            QQC2.Label {
                                text: controller.batteryTimeRemainingText || KI18n.i18n("Calculating…")
                                font.weight: Font.DemiBold
                                font.pointSize: batteryPage.theme.fontBody * 1.15
                                color: batteryPage.theme.textColor
                                elide: Text.ElideRight
                            }
                        }
                    }

                    // Power Draw (Watts)
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: wattCol.implicitHeight + 14
                        radius: theme.innerRadius
                        color: theme.cardNestedBg
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        ColumnLayout {
                            id: wattCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            QQC2.Label {
                                text: KI18n.i18n("Discharge Rate")
                                font.pointSize: batteryPage.theme.fontCaption
                                color: batteryPage.theme.mutedTextColor
                            }
                            QQC2.Label {
                                text: controller.batteryPowerAvailable ? (controller.batteryWatts.toFixed(1) + " W") : KI18n.i18n("Unavailable")
                                font.weight: Font.DemiBold
                                font.pointSize: batteryPage.theme.fontBody * 1.15
                                color: batteryPage.theme.textColor
                            }
                        }
                    }

                    // Battery Health
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: hlthCol.implicitHeight + 14
                        radius: theme.innerRadius
                        color: theme.cardNestedBg
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        ColumnLayout {
                            id: hlthCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            QQC2.Label {
                                text: KI18n.i18n("Battery Health")
                                font.pointSize: batteryPage.theme.fontCaption
                                color: batteryPage.theme.mutedTextColor
                            }
                            QQC2.Label {
                                text: controller.batteryHealthAvailable ? (Math.round(controller.batteryHealthPercent) + "%") : KI18n.i18n("Good")
                                font.weight: Font.DemiBold
                                font.pointSize: batteryPage.theme.fontBody * 1.15
                                color: batteryPage.theme.positiveColor
                            }
                        }
                    }

                    // Charging State
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: stCol.implicitHeight + 14
                        radius: theme.innerRadius
                        color: theme.cardNestedBg
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        ColumnLayout {
                            id: stCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            QQC2.Label {
                                text: KI18n.i18n("Current State")
                                font.pointSize: batteryPage.theme.fontCaption
                                color: batteryPage.theme.mutedTextColor
                            }
                            QQC2.Label {
                                text: controller.onAcPower ? KI18n.i18n("Plugged In") : KI18n.i18n("On Battery")
                                font.weight: Font.DemiBold
                                font.pointSize: batteryPage.theme.fontBody * 1.15
                                color: batteryPage.theme.textColor
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 3. DETECTED BATTERIES & PERIPHERALS CARD
        // ==========================================
        PulseCard {
            Layout.fillWidth: true
            padding: theme.spacingL

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Kirigami.Icon {
                        source: "battery-charging"
                        implicitWidth: 16
                        implicitHeight: 16
                        color: theme.batteryAccent
                    }
                    Kirigami.Heading {
                        level: 3
                        text: KI18n.i18n("Connected Batteries & Peripherals")
                        Layout.fillWidth: true
                    }
                }

                QQC2.Label {
                    visible: (controller.batteries || []).length === 0
                    text: KI18n.i18n("No readable batteries present")
                    color: Kirigami.Theme.disabledTextColor
                }

                Repeater {
                    model: controller.batteries || []
                    delegate: Rectangle {
                        id: batDeviceItem
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: batDevRow.implicitHeight + 14
                        radius: theme.innerRadius
                        color: theme.cardNestedBg
                        border.width: 1
                        border.color: theme.subtleBorderColor

                        RowLayout {
                            id: batDevRow
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10

                            Kirigami.Icon {
                                source: batDeviceItem.modelData.name.indexOf("hidpp") >= 0 ? "input-mouse" : "battery"
                                implicitWidth: 18
                                implicitHeight: 18
                                color: theme.batteryAccent
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                QQC2.Label {
                                    text: (batDeviceItem.modelData.name.indexOf("BAT") === 0 ? "Internal Battery (" + batDeviceItem.modelData.name + ")" : (batDeviceItem.modelData.name.indexOf("hidpp") === 0 ? "Wireless Peripheral (" + batDeviceItem.modelData.name + ")" : batDeviceItem.modelData.name))
                                    font.weight: Font.DemiBold
                                    color: batteryPage.theme.textColor
                                }
                                QQC2.Label {
                                    text: (batDeviceItem.modelData.levelAvailable && isFinite(Number(batDeviceItem.modelData.percent)) ? Number(batDeviceItem.modelData.percent).toFixed(0) + "% laddning" : KI18n.i18n("Laddningsnivå ej tillgänglig"))
                                          + " · " + (batDeviceItem.modelData.powerAvailable && isFinite(Number(batDeviceItem.modelData.powerWatts)) ? Number(batDeviceItem.modelData.powerWatts).toFixed(1) + " W" : KI18n.i18n("Effektförbrukning ej tillgänglig"))
                                    font.pointSize: batteryPage.theme.fontCaption
                                    color: batteryPage.theme.mutedTextColor
                                }
                            }

                            Rectangle {
                                radius: 4
                                color: batDeviceItem.modelData.charging ? theme.positiveColor : theme.subtleBorderColor
                                implicitHeight: 20
                                implicitWidth: stateLbl.implicitWidth + 12
                                QQC2.Label {
                                    id: stateLbl
                                    anchors.centerIn: parent
                                    text: batDeviceItem.modelData.charging ? KI18n.i18n("Charging") : (batDeviceItem.modelData.discharging ? KI18n.i18n("Discharging") : KI18n.i18n("Idle / Ready"))
                                    font.pointSize: batteryPage.theme.fontCaption * 0.9
                                    font.weight: Font.DemiBold
                                    color: batDeviceItem.modelData.charging ? "#ffffff" : batteryPage.theme.mutedTextColor
                                }
                            }
                        }
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pointSize: batteryPage.theme.fontCaption
                    color: batteryPage.theme.faintTextColor
                    text: KI18n.i18n("Includes internal system battery packs and wireless accessories reporting battery status through HID++ / Bluetooth.")
                }
            }
        }
    }
}
