// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

// Navigation sidebar following KDE Breeze design principles
Rectangle {
    id: sidebar

    required property PulseTheme theme
    required property var controller
    property string currentPage: "overview"

    signal pageSelected(string pageId, string section)

    width: 220
    color: sidebar.theme.surfaceColor

    // Subtle right border separating navigation from page content
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: sidebar.theme.cardBorderColor
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: sidebar.theme.spacingM
        spacing: sidebar.theme.spacingS

        // App title header
        RowLayout {
            Layout.fillWidth: true
            spacing: sidebar.theme.spacingS

            Kirigami.Icon {
                source: "utilities-system-monitor"
                implicitWidth: 20
                implicitHeight: 20
                color: sidebar.theme.highlightColor
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: KI18n.i18nd("loofi-pulse", "Loofi Pulse")
                font.pointSize: sidebar.theme.fontTitle
                font.weight: Font.DemiBold
                color: sidebar.theme.textColor
                elide: Text.ElideRight
            }
        }

        Item {
            Layout.preferredHeight: sidebar.theme.spacingS
        }

        // Navigation Items List (7 Core Views)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            NavButton {
                pageId: "overview"
                label: KI18n.i18nd("loofi-pulse", "Overview")
                iconName: "go-home"
                isCurrent: sidebar.currentPage === "overview"
                onClicked: sidebar.pageSelected("overview", "")
            }

            NavButton {
                pageId: "pressure"
                label: KI18n.i18nd("loofi-pulse", "Memory & Pressure")
                iconName: "speedometer"
                isCurrent: sidebar.currentPage === "pressure"
                onClicked: sidebar.pageSelected("pressure", "overview")
            }

            NavButton {
                pageId: "power"
                label: KI18n.i18nd("loofi-pulse", "Power & Cooling")
                iconName: "battery-profile-performance"
                isCurrent: sidebar.currentPage === "power"
                onClicked: sidebar.pageSelected("power", "cpu")
            }

            NavButton {
                pageId: "storage"
                label: KI18n.i18nd("loofi-pulse", "Storage & Network")
                iconName: "drive-harddisk"
                isCurrent: sidebar.currentPage === "storage"
                onClicked: sidebar.pageSelected("storage", "")
            }

            NavButton {
                pageId: "processes"
                label: KI18n.i18nd("loofi-pulse", "Processes")
                iconName: "view-process-all"
                isCurrent: sidebar.currentPage === "processes"
                onClicked: sidebar.pageSelected("processes", "")
            }

            NavButton {
                pageId: "flight"
                label: KI18n.i18nd("loofi-pulse", "History")
                iconName: "chronometer"
                isCurrent: sidebar.currentPage === "flight"
                onClicked: sidebar.pageSelected("flight", "")
            }

            NavButton {
                pageId: "settings"
                label: KI18n.i18nd("loofi-pulse", "Settings")
                iconName: "settings-configure"
                isCurrent: sidebar.currentPage === "settings"
                onClicked: sidebar.pageSelected("settings", "")
            }
        }

        Item {
            Layout.fillHeight: true
        }

        // Dynamic Host System Info Badge
        Rectangle {
            Layout.fillWidth: true
            radius: sidebar.theme.innerRadius
            color: sidebar.theme.cardNestedBg
            border.width: 1
            border.color: sidebar.theme.subtleBorderColor
            implicitHeight: sysInfoRow.implicitHeight + sidebar.theme.spacingM * 2

            RowLayout {
                id: sysInfoRow
                anchors.fill: parent
                anchors.margins: sidebar.theme.spacingM
                spacing: sidebar.theme.spacingS

                Kirigami.Icon {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    source: "distributor-logo-fedora"
                    fallback: "computer"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    QQC2.Label {
                        text: sidebar.controller ? sidebar.controller.hostDistroName : "Linux"
                        font.weight: Font.Medium
                        font.pointSize: sidebar.theme.fontBody
                        color: sidebar.theme.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    QQC2.Label {
                        text: sidebar.controller ? sidebar.controller.hostKernelVersion : ""
                        font.pointSize: sidebar.theme.fontCaption
                        color: sidebar.theme.faintTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    QQC2.Label {
                        text: sidebar.controller ? sidebar.controller.hostDeviceModel : ""
                        font.pointSize: sidebar.theme.fontCaption
                        color: sidebar.theme.mutedTextColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        visible: text.length > 0
                    }
                }
            }
        }
    }

    component NavButton: Rectangle {
        id: btn
        property string pageId
        property string label
        property string iconName
        property bool isCurrent: false
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 36
        radius: sidebar.theme.innerRadius
        color: isCurrent ? sidebar.theme.selectionColor : (btnHover.hovered ? sidebar.theme.subtleSurfaceColor : "transparent")
        border.width: isCurrent ? 1 : 0
        border.color: isCurrent ? sidebar.theme.highlightColor : "transparent"

        Accessible.role: Accessible.Button
        Accessible.name: label

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: sidebar.theme.spacingS

            Kirigami.Icon {
                source: btn.iconName
                implicitWidth: 16
                implicitHeight: 16
                color: btn.isCurrent ? sidebar.theme.highlightColor : (btnHover.hovered ? sidebar.theme.textColor : sidebar.theme.mutedTextColor)
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: btn.label
                font.pointSize: sidebar.theme.fontBody
                font.weight: btn.isCurrent ? Font.Medium : Font.Normal
                color: btn.isCurrent ? sidebar.theme.textColor : (btnHover.hovered ? sidebar.theme.textColor : sidebar.theme.mutedTextColor)
                elide: Text.ElideRight
            }
        }

        HoverHandler { id: btnHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: btn.clicked() }
    }
}
