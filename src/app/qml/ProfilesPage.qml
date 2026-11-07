// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

Kirigami.ScrollablePage {
    id: profilesPage
    objectName: "profilesPage"
    title: KI18n.i18n("Profiles")
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
        bgColor: profilesPage.Kirigami.Theme.backgroundColor
        disabledColor: profilesPage.Kirigami.Theme.disabledTextColor
        negativeColor: profilesPage.Kirigami.Theme.negativeTextColor
        neutralColor: profilesPage.Kirigami.Theme.neutralTextColor
        positiveColor: profilesPage.Kirigami.Theme.positiveTextColor
        textColor: profilesPage.Kirigami.Theme.textColor
        highlightColor: profilesPage.Kirigami.Theme.highlightColor
    }

    function profileIcon(prof) {
        if (prof === "performance") return "speedometer"
        if (prof === "power-saver") return "battery-profile-powersave"
        return "battery-profile-performance"
    }

    function profileColor(prof) {
        if (prof === "performance") return theme.cpuAccent
        if (prof === "power-saver") return theme.positiveColor
        return theme.highlightColor
    }

    ColumnLayout {
        width: profilesPage.width - Kirigami.Units.largeSpacing * 2
        spacing: Kirigami.Units.largeSpacing

        // ==========================================
        // 1. PAGE HEADER STRIP
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            spacing: profilesPage.theme.spacingM

            ColumnLayout {
                spacing: 2
                QQC2.Label {
                    text: KI18n.i18n("Power Profiles")
                    font.weight: Font.DemiBold
                    font.pointSize: profilesPage.theme.fontHeadline
                    color: profilesPage.theme.textColor
                }
                QQC2.Label {
                    text: KI18n.i18n("System performance, energy conservation, and power-profiles-daemon holds.")
                    font.pointSize: profilesPage.theme.fontCaption
                    color: profilesPage.theme.mutedTextColor
                }
            }

            Item { Layout.fillWidth: true }

            // Active Profile Pill
            Rectangle {
                radius: profilesPage.theme.pillRadius
                color: profilesPage.theme.cardNestedBg
                border.width: 1
                border.color: profilesPage.theme.subtleBorderColor
                implicitHeight: 26
                implicitWidth: activePillRow.implicitWidth + 18

                RowLayout {
                    id: activePillRow
                    anchors.centerIn: parent
                    spacing: 6
                    Kirigami.Icon {
                        source: profilesPage.profileIcon(controller.activePowerProfile)
                        implicitWidth: 12
                        implicitHeight: 12
                        color: profilesPage.profileColor(controller.activePowerProfile)
                    }
                    QQC2.Label {
                        text: (controller.activePowerProfile ? controller.activePowerProfile.toUpperCase() : "BALANCED")
                        font.bold: true
                        font.pointSize: profilesPage.theme.fontCaption
                        color: profilesPage.profileColor(controller.activePowerProfile)
                    }
                }
            }
        }

        // ==========================================
        // 2. INTERACTIVE POWER PROFILE HERO TILES
        // ==========================================
        PulseCard {
            Layout.fillWidth: true
            padding: theme.spacingL
            accentColor: theme.powerAccent
            accentStrength: 0.10

            ColumnLayout {
                id: profCol
                objectName: "section_profile"
                focus: true
                Accessible.name: KI18n.i18n("profile section")
                width: parent.width
                spacing: Kirigami.Units.mediumSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        radius: 6
                        color: theme.accentTint(theme.powerAccent, theme.isDark ? 0.22 : 0.14)
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            source: "configure-shortcuts"
                            color: theme.powerAccent
                        }
                    }
                    ColumnLayout {
                        spacing: 1
                        QQC2.Label {
                            text: KI18n.i18n("Välj aktiv systemprofil")
                            font.weight: Font.DemiBold
                            font.pointSize: profilesPage.theme.fontTitle
                            color: profilesPage.theme.textColor
                        }
                        QQC2.Label {
                            text: controller.powerProfileServiceAvailable
                                  ? KI18n.i18n("Systemet anpassar klockfrekvenser, spänningar och fläktstyrning direkt.")
                                  : KI18n.i18n("power-profiles-daemon är inte aktivt på detta system.")
                            font.pointSize: profilesPage.theme.fontCaption
                            color: profilesPage.theme.mutedTextColor
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                // 3 Hero Selection Tiles
                GridLayout {
                    Layout.fillWidth: true
                    columns: profilesPage.width < 780 ? 1 : 3
                    columnSpacing: Kirigami.Units.mediumSpacing
                    rowSpacing: Kirigami.Units.mediumSpacing

                    // 1. Power-Saver
                    Rectangle {
                        id: tileSaver
                        readonly property bool isCurrent: controller.activePowerProfile === "power-saver"
                        Layout.fillWidth: true
                        implicitHeight: tileSaverCol.implicitHeight + 24
                        radius: theme.innerRadius
                        color: isCurrent ? theme.selectionColor : (tileSaverHov.hovered ? theme.subtleSurfaceColor : theme.cardNestedBg)
                        border.width: isCurrent ? 2 : 1
                        border.color: isCurrent ? theme.positiveColor : theme.subtleBorderColor

                        ColumnLayout {
                            id: tileSaverCol
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Kirigami.Icon {
                                    source: "battery-profile-powersave"
                                    implicitWidth: 20
                                    implicitHeight: 20
                                    color: theme.positiveColor
                                }
                                QQC2.Label {
                                    text: KI18n.i18n("Strömsparläge")
                                    font.weight: Font.DemiBold
                                    font.pointSize: profilesPage.theme.fontBody * 1.05
                                    color: profilesPage.theme.textColor
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    visible: tileSaver.isCurrent
                                    radius: 4
                                    color: theme.positiveColor
                                    implicitHeight: 18
                                    implicitWidth: activeSaverLbl.implicitWidth + 8
                                    QQC2.Label {
                                        id: activeSaverLbl
                                        anchors.centerIn: parent
                                        text: KI18n.i18n("Aktiv")
                                        font.pointSize: profilesPage.theme.fontCaption * 0.85
                                        font.weight: Font.Bold
                                        color: "#ffffff"
                                    }
                                }
                            }

                            QQC2.Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pointSize: profilesPage.theme.fontCaption
                                color: profilesPage.theme.mutedTextColor
                                text: KI18n.i18n("Maximerar batteritid och håller fläktarna tysta genom att strypa toppfrekvenser och sänka effektgränser.")
                            }
                        }

                        HoverHandler { id: tileSaverHov; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            enabled: controller.powerProfileServiceAvailable && !controller.powerProfileChangePending
                            onTapped: controller.setPowerProfile("power-saver")
                        }
                    }

                    // 2. Balanced
                    Rectangle {
                        id: tileBalanced
                        readonly property bool isCurrent: controller.activePowerProfile === "balanced"
                        Layout.fillWidth: true
                        implicitHeight: tileBalCol.implicitHeight + 24
                        radius: theme.innerRadius
                        color: isCurrent ? theme.selectionColor : (tileBalHov.hovered ? theme.subtleSurfaceColor : theme.cardNestedBg)
                        border.width: isCurrent ? 2 : 1
                        border.color: isCurrent ? theme.highlightColor : theme.subtleBorderColor

                        ColumnLayout {
                            id: tileBalCol
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Kirigami.Icon {
                                    source: "battery-profile-performance"
                                    implicitWidth: 20
                                    implicitHeight: 20
                                    color: theme.highlightColor
                                }
                                QQC2.Label {
                                    text: KI18n.i18n("Balanserat")
                                    font.weight: Font.DemiBold
                                    font.pointSize: profilesPage.theme.fontBody * 1.05
                                    color: profilesPage.theme.textColor
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    visible: tileBalanced.isCurrent
                                    radius: 4
                                    color: theme.highlightColor
                                    implicitHeight: 18
                                    implicitWidth: activeBalLbl.implicitWidth + 8
                                    QQC2.Label {
                                        id: activeBalLbl
                                        anchors.centerIn: parent
                                        text: KI18n.i18n("Aktiv")
                                        font.pointSize: profilesPage.theme.fontCaption * 0.85
                                        font.weight: Font.Bold
                                        color: "#ffffff"
                                    }
                                }
                            }

                            QQC2.Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pointSize: profilesPage.theme.fontCaption
                                color: profilesPage.theme.mutedTextColor
                                text: KI18n.i18n("Standardläge med optimal balans. Ökar klockfrekvenser dynamiskt vid behov och återgår snabbt till låg effekt.")
                            }
                        }

                        HoverHandler { id: tileBalHov; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            enabled: controller.powerProfileServiceAvailable && !controller.powerProfileChangePending
                            onTapped: controller.setPowerProfile("balanced")
                        }
                    }

                    // 3. Performance
                    Rectangle {
                        id: tilePerf
                        readonly property bool isCurrent: controller.activePowerProfile === "performance"
                        Layout.fillWidth: true
                        implicitHeight: tilePerfCol.implicitHeight + 24
                        radius: theme.innerRadius
                        color: isCurrent ? theme.selectionColor : (tilePerfHov.hovered ? theme.subtleSurfaceColor : theme.cardNestedBg)
                        border.width: isCurrent ? 2 : 1
                        border.color: isCurrent ? theme.cpuAccent : theme.subtleBorderColor

                        ColumnLayout {
                            id: tilePerfCol
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Kirigami.Icon {
                                    source: "speedometer"
                                    implicitWidth: 20
                                    implicitHeight: 20
                                    color: theme.cpuAccent
                                }
                                QQC2.Label {
                                    text: KI18n.i18n("Hög prestanda")
                                    font.weight: Font.DemiBold
                                    font.pointSize: profilesPage.theme.fontBody * 1.05
                                    color: profilesPage.theme.textColor
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    visible: tilePerf.isCurrent
                                    radius: 4
                                    color: theme.cpuAccent
                                    implicitHeight: 18
                                    implicitWidth: activePerfLbl.implicitWidth + 8
                                    QQC2.Label {
                                        id: activePerfLbl
                                        anchors.centerIn: parent
                                        text: KI18n.i18n("Aktiv")
                                        font.pointSize: profilesPage.theme.fontCaption * 0.85
                                        font.weight: Font.Bold
                                        color: "#ffffff"
                                    }
                                }
                            }

                            QQC2.Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pointSize: profilesPage.theme.fontCaption
                                color: profilesPage.theme.mutedTextColor
                                text: KI18n.i18n("Prioriterar omedelbar responsivitet och maximal turbo-boost. Fläktar tillåts varva upp för aggressiv kylning.")
                            }
                        }

                        HoverHandler { id: tilePerfHov; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            enabled: controller.powerProfileServiceAvailable && !controller.powerProfileChangePending
                            onTapped: controller.setPowerProfile("performance")
                        }
                    }
                }
            }
        }

        // ==========================================
        // 3. PROFILE HOLDS & SYSTEM INTEGRATION CARD
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
                        source: "security-high"
                        implicitWidth: 16
                        implicitHeight: 16
                        color: theme.neutralColor
                    }
                    Kirigami.Heading {
                        level: 3
                        text: KI18n.i18n("Aktiva profilspärrar (Holds)")
                        Layout.fillWidth: true
                    }
                }

                QQC2.Label {
                    visible: !controller.powerProfileHoldsSummary || controller.powerProfileHoldsSummary.length === 0
                    text: KI18n.i18n("Inga applikationer har låst systemets energiprofil. Användarval gäller fritt.")
                    color: profilesPage.theme.mutedTextColor
                    font.pointSize: profilesPage.theme.fontCaption
                }

                Rectangle {
                    visible: controller.powerProfileHoldsSummary && controller.powerProfileHoldsSummary.length > 0
                    Layout.fillWidth: true
                    radius: theme.innerRadius
                    color: theme.cardNestedBg
                    border.width: 1
                    border.color: theme.neutralColor
                    implicitHeight: holdsInfoRow.implicitHeight + 16

                    RowLayout {
                        id: holdsInfoRow
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8
                        Kirigami.Icon {
                            source: "dialog-warning"
                            implicitWidth: 16
                            implicitHeight: 16
                            color: theme.neutralColor
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: KI18n.i18n("Aktiva lås från appar: ") + controller.powerProfileHoldsSummary
                            font.pointSize: profilesPage.theme.fontBody
                            color: profilesPage.theme.textColor
                        }
                    }
                }
            }
        }
    }
}
