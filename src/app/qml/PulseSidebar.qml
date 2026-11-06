// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

// PulseSidebar: Fixed navigation sidebar inspired by KDE System Settings and Discover.
// Expands to full sidebar (224px) on wide displays and collapses to icon-only (56px) on medium displays.
Rectangle {
    id: sidebar

    required property PulseTheme theme
    required property var controller
    property string currentPage: "overview"
    property bool collapsed: width < 120

    signal pageSelected(string pageId)

    width: collapsed ? 56 : 224
    color: theme.isDark ? Qt.rgba(theme.bgColor.r * 0.85, theme.bgColor.g * 0.85, theme.bgColor.b * 0.85, 1.0) : Qt.rgba(0.92, 0.93, 0.94, 1.0)

    Behavior on width {
        NumberAnimation { duration: sidebar.theme.durationShort; easing.type: Easing.OutCubic }
    }

    // Right border line
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: sidebar.theme.cardBorderColor
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: sidebar.collapsed ? 4 : 8
        spacing: 2

        // Top Brand Area
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 52

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: sidebar.collapsed ? 6 : 8
                spacing: 10

                Image {
                    source: "file:///home/loofi/Skrivbord/Loofi Work/plasma-hardware-pulse/assets/logo.svg"
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    sourceSize.width: 56
                    sourceSize.height: 56
                    fillMode: Image.PreserveAspectFit
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 0
                    visible: !sidebar.collapsed

                    QQC2.Label {
                        text: "Loofi Pulse"
                        font.pointSize: sidebar.theme.fontBody
                        font.weight: Font.Bold
                        color: sidebar.theme.textColor
                    }

                    QQC2.Label {
                        text: "v0.15.0"
                        font.pointSize: sidebar.theme.fontCaption * 0.9
                        color: sidebar.theme.faintTextColor
                    }
                }
            }
        }

        // Navigation Menu Items
        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 2

                // Section 1: Overview
                NavButton {
                    pageId: "overview"
                    label: KI18n.i18nd("loofi-pulse", "Overview")
                    iconName: "view-statistics"
                    badgeCount: sidebar.controller && sidebar.controller.verdictSeverity > 0 ? 1 : 0
                    badgeTone: sidebar.controller ? sidebar.theme.severityColor(sidebar.controller.verdictSeverity) : sidebar.theme.neutralColor
                    isCurrent: sidebar.currentPage === "overview"
                    collapsed: sidebar.collapsed
                    theme: sidebar.theme
                    onClicked: sidebar.pageSelected("overview")
                }

                // Section: Hardware
                NavHeader {
                    title: KI18n.i18nd("loofi-pulse", "HARDWARE")
                    visible: !sidebar.collapsed
                    theme: sidebar.theme
                }

                NavButton {
                    pageId: "power"
                    label: KI18n.i18nd("loofi-pulse", "Power & Thermal")
                    iconName: "battery-profile-performance"
                    isCurrent: sidebar.currentPage === "power"
                    collapsed: sidebar.collapsed
                    theme: sidebar.theme
                    onClicked: sidebar.pageSelected("power")
                }

                NavButton {
                    pageId: "pressure"
                    label: KI18n.i18nd("loofi-pulse", "Memory & Pressure")
                    iconName: "memory"
                    isCurrent: sidebar.currentPage === "pressure"
                    collapsed: sidebar.collapsed
                    theme: sidebar.theme
                    onClicked: sidebar.pageSelected("pressure")
                }

                NavButton {
                    pageId: "storage"
                    label: KI18n.i18nd("loofi-pulse", "Storage & Network")
                    iconName: "drive-harddisk"
                    isCurrent: sidebar.currentPage === "storage"
                    collapsed: sidebar.collapsed
                    theme: sidebar.theme
                    onClicked: sidebar.pageSelected("storage")
                }

                // Section: System
                NavHeader {
                    title: KI18n.i18nd("loofi-pulse", "SYSTEM")
                    visible: !sidebar.collapsed
                    theme: sidebar.theme
                }

                NavButton {
                    pageId: "processes"
                    label: KI18n.i18nd("loofi-pulse", "Processes")
                    iconName: "view-process-all"
                    isCurrent: sidebar.currentPage === "processes"
                    collapsed: sidebar.collapsed
                    theme: sidebar.theme
                    onClicked: sidebar.pageSelected("processes")
                }

                // Section: Analysis
                NavHeader {
                    title: KI18n.i18nd("loofi-pulse", "ANALYSIS")
                    visible: !sidebar.collapsed
                    theme: sidebar.theme
                }

                NavButton {
                    pageId: "flight"
                    label: KI18n.i18nd("loofi-pulse", "Flight Recorder")
                    iconName: "chronometer"
                    isCurrent: sidebar.currentPage === "flight"
                    collapsed: sidebar.collapsed
                    theme: sidebar.theme
                    onClicked: sidebar.pageSelected("flight")
                }

                // Spacer
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 12
                }

                // Settings
                NavButton {
                    pageId: "settings"
                    label: KI18n.i18nd("loofi-pulse", "Settings")
                    iconName: "settings-configure"
                    isCurrent: sidebar.currentPage === "settings"
                    collapsed: sidebar.collapsed
                    theme: sidebar.theme
                    onClicked: sidebar.pageSelected("settings")
                }
            }
        }

        // Bottom Footer: Collector Status & Pulse Dot
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 40

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: sidebar.theme.subtleBorderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: sidebar.collapsed ? 14 : 10
                anchors.rightMargin: 8
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8
                    radius: 4
                    color: sidebar.controller && sidebar.controller.collectorConnected
                           ? (sidebar.controller.recordingPaused ? sidebar.theme.neutralColor : sidebar.theme.positiveColor)
                           : sidebar.theme.negativeColor

                    // Optional pulse animation if active and motion allowed
                    SequentialAnimation on opacity {
                        running: sidebar.theme.motionAllowed && sidebar.controller && sidebar.controller.collectorConnected && !sidebar.controller.recordingPaused
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.4; duration: 900; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1.0; duration: 900; easing.type: Easing.InOutQuad }
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    visible: !sidebar.collapsed
                    text: sidebar.controller
                          ? (sidebar.controller.recordingPaused
                             ? KI18n.i18nd("loofi-pulse", "Paused")
                             : (KI18n.i18nd("loofi-pulse", "Live") + " · " + (sidebar.controller.pollIntervalMs / 1000).toFixed(1) + " s"))
                          : KI18n.i18nd("loofi-pulse", "Connecting…")
                    font.pointSize: sidebar.theme.fontCaption
                    color: sidebar.theme.mutedTextColor
                    elide: Text.ElideRight
                }
            }

            QQC2.ToolTip.text: sidebar.controller ? sidebar.controller.collectorState : ""
            QQC2.ToolTip.visible: footerHover.hovered
            HoverHandler { id: footerHover }
        }
    }

    // Helper component for Section Headers
    component NavHeader: Item {
        id: navHeaderItem
        property string title
        property PulseTheme theme
        Layout.fillWidth: true
        Layout.preferredHeight: 28

        QQC2.Label {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: navHeaderItem.title
            font.pointSize: (navHeaderItem.theme ? navHeaderItem.theme.fontCaption : sidebar.theme.fontCaption) * 0.82
            font.weight: Font.Bold
            font.letterSpacing: 0.8
            color: navHeaderItem.theme ? navHeaderItem.theme.faintTextColor : sidebar.theme.faintTextColor
        }
    }

    // Helper component for Navigation Buttons
    component NavButton: Rectangle {
        id: btn
        property string pageId
        property string label
        property string iconName
        property int badgeCount: 0
        property color badgeTone: theme.neutralColor
        property bool isCurrent: false
        property bool collapsed: false
        property PulseTheme theme
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 36
        radius: theme.innerRadius
        color: isCurrent
               ? theme.selectionColor
               : (navHover.hovered ? theme.subtleSurfaceColor : "transparent")

        Accessible.role: Accessible.Button
        Accessible.name: label

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: btn.collapsed ? 12 : 10
            anchors.rightMargin: 10
            spacing: 10

            Kirigami.Icon {
                source: btn.iconName
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                color: btn.isCurrent ? btn.theme.highlightColor : (navHover.hovered ? btn.theme.textColor : btn.theme.mutedTextColor)
            }

            QQC2.Label {
                Layout.fillWidth: true
                visible: !btn.collapsed
                text: btn.label
                font.pointSize: btn.theme.fontBody
                font.weight: btn.isCurrent ? Font.DemiBold : Font.Normal
                color: btn.isCurrent ? btn.theme.highlightColor : btn.theme.textColor
                elide: Text.ElideRight
            }

            // Warning Badge
            Rectangle {
                visible: !btn.collapsed && btn.badgeCount > 0
                radius: 999
                color: btn.badgeTone
                implicitWidth: badgeLbl.implicitWidth + 10
                implicitHeight: badgeLbl.implicitHeight + 2

                QQC2.Label {
                    id: badgeLbl
                    anchors.centerIn: parent
                    text: btn.badgeCount.toString()
                    font.pointSize: btn.theme.fontCaption * 0.85
                    font.weight: Font.Bold
                    color: "#ffffff"
                }
            }
        }

        QQC2.ToolTip.text: btn.label
        QQC2.ToolTip.visible: btn.collapsed && navHover.hovered

        HoverHandler { id: navHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: btn.clicked() }
    }
}
