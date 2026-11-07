// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

// Navigation sidebar following KDE Breeze design principles with structured category hierarchy
Rectangle {
    id: sidebar

    required property PulseTheme theme
    required property var controller
    property string currentPage: "overview"
    property string currentSection: ""
    property bool powerExpanded: currentPage === "power"

    onCurrentPageChanged: {
        if (currentPage === "power") {
            powerExpanded = true
        }
    }

    signal pageSelected(string pageId, string section)

    width: 228
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

        // Navigation Items List
        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 2

                // SECTION 1: SYSTEM
                NavHeader {
                    title: KI18n.i18n("SYSTEM")
                }

                NavButton {
                    pageId: "overview"
                    label: KI18n.i18nd("loofi-pulse", "Overview")
                    iconName: "go-home"
                    isCurrent: sidebar.currentPage === "overview"
                    onClicked: {
                        sidebar.currentSection = ""
                        sidebar.pageSelected("overview", "")
                    }
                }

                NavButton {
                    pageId: "pressure"
                    label: KI18n.i18nd("loofi-pulse", "Memory & Pressure")
                    iconName: "speedometer"
                    isCurrent: sidebar.currentPage === "pressure"
                    onClicked: {
                        sidebar.currentSection = ""
                        sidebar.pageSelected("pressure", "overview")
                    }
                }

                NavButton {
                    pageId: "power"
                    label: KI18n.i18nd("loofi-pulse", "Power & Cooling")
                    iconName: "battery-profile-performance"
                    isCurrent: sidebar.currentPage === "power"
                    onClicked: {
                        sidebar.currentSection = ""
                        sidebar.pageSelected("power", "")
                    }
                }

                Item { Layout.preferredHeight: 6 }

                // SECTION 2: HARDWARE & DEVICES
                NavHeader {
                    title: KI18n.i18n("HARDWARE & DEVICES")
                }

                NavButton {
                    pageId: "gpu"
                    label: KI18n.i18nd("loofi-pulse", "Graphics (GPU)")
                    iconName: "video-display"
                    isCurrent: sidebar.currentPage === "gpu"
                    onClicked: {
                        sidebar.currentSection = ""
                        sidebar.pageSelected("gpu", "")
                    }
                }

                NavButton {
                    pageId: "battery"
                    label: KI18n.i18nd("loofi-pulse", "Battery")
                    iconName: "battery-charging"
                    isCurrent: sidebar.currentPage === "battery"
                    onClicked: {
                        sidebar.currentSection = ""
                        sidebar.pageSelected("battery", "")
                    }
                }

                NavButton {
                    pageId: "sensors"
                    label: KI18n.i18n("Sensors")
                    iconName: "temperature-symbolic"
                    isCurrent: sidebar.currentPage === "sensors"
                    onClicked: {
                        sidebar.currentSection = ""
                        sidebar.pageSelected("sensors", "")
                    }
                }

                NavButton {
                    pageId: "storage"
                    label: KI18n.i18nd("loofi-pulse", "Storage & Network")
                    iconName: "drive-harddisk"
                    isCurrent: sidebar.currentPage === "storage"
                    onClicked: {
                        sidebar.currentSection = ""
                        sidebar.pageSelected("storage", "")
                    }
                }

                NavButton {
                    pageId: "processes"
                    label: KI18n.i18nd("loofi-pulse", "Processes")
                    iconName: "view-process-all"
                    isCurrent: sidebar.currentPage === "processes"
                    onClicked: {
                        sidebar.currentSection = ""
                        sidebar.pageSelected("processes", "")
                    }
                }

                Item { Layout.preferredHeight: 6 }

                // SECTION 3: CONTROL & PROFILES
                NavHeader {
                    title: KI18n.i18n("CONTROL")
                }

                NavButton {
                    pageId: "profiles"
                    label: KI18n.i18n("Profiles")
                    iconName: "configure-shortcuts"
                    isCurrent: sidebar.currentPage === "profiles"
                    onClicked: {
                        sidebar.currentSection = ""
                        sidebar.pageSelected("profiles", "")
                    }
                }

                Item { Layout.preferredHeight: 6 }

                // SECTION 4: FLIGHT RECORDER & TIMELINE
                NavHeader {
                    title: KI18n.i18n("FLIGHT RECORDER")
                }

                NavButton {
                    pageId: "flight"
                    label: KI18n.i18nd("loofi-pulse", "History")
                    iconName: "chronometer"
                    isCurrent: sidebar.currentPage === "flight"
                    onClicked: {
                        sidebar.currentSection = ""
                        sidebar.pageSelected("flight", "")
                    }
                }
            }
        }

        // Pinned Settings Section Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: sidebar.theme.subtleBorderColor
        }

        NavButton {
            pageId: "settings"
            label: KI18n.i18nd("loofi-pulse", "Settings")
            iconName: "settings-configure"
            isCurrent: sidebar.currentPage === "settings"
            onClicked: {
                sidebar.currentSection = ""
                sidebar.pageSelected("settings", "")
            }
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

    // Category Header Component
    component NavHeader: Item {
        id: headerItem
        property string title
        Layout.fillWidth: true
        Layout.preferredHeight: 22

        QQC2.Label {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: headerItem.title
            font.pointSize: sidebar.theme.fontCaption * 0.82
            font.weight: Font.DemiBold
            font.letterSpacing: 0.8
            color: sidebar.theme.faintTextColor
        }
    }

    // Top-Level Navigation Button Component
    component NavButton: Rectangle {
        id: btn
        property string pageId
        property string label
        property string iconName
        property bool isCurrent: false
        property bool hasSubmenu: false
        property bool submenuExpanded: false
        signal clicked()
        signal toggleSubmenu()

        Layout.fillWidth: true
        Layout.preferredHeight: 34
        radius: sidebar.theme.innerRadius
        color: isCurrent ? sidebar.theme.selectionColor : (btnHover.hovered ? sidebar.theme.subtleSurfaceColor : "transparent")
        border.width: isCurrent ? 1 : 0
        border.color: isCurrent ? sidebar.theme.highlightColor : "transparent"

        Accessible.role: Accessible.Button
        Accessible.name: label

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 8
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

            // Submenu chevron
            Kirigami.Icon {
                visible: btn.hasSubmenu
                source: btn.submenuExpanded ? "arrow-down-s" : "arrow-right-s"
                implicitWidth: 12
                implicitHeight: 12
                color: sidebar.theme.faintTextColor
            }
        }

        HoverHandler { id: btnHover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: btn.clicked()
        }
    }

    // Submenu Navigation Button Component
    component SubNavButton: Rectangle {
        id: subBtn
        property string sectionId
        property string label
        property string iconName
        property bool isCurrent: sidebar.currentPage === "power" && sidebar.currentSection === subBtn.sectionId
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 30
        Layout.leftMargin: 16
        radius: sidebar.theme.innerRadius
        color: isCurrent ? sidebar.theme.selectionColor : (subHover.hovered ? sidebar.theme.subtleSurfaceColor : "transparent")
        border.width: isCurrent ? 1 : 0
        border.color: isCurrent ? sidebar.theme.highlightColor : "transparent"

        Accessible.role: Accessible.Button
        Accessible.name: label

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 8

            // Subtle vertical tree guide tick
            Rectangle {
                width: 2
                height: subBtn.isCurrent ? 14 : 8
                radius: 1
                color: subBtn.isCurrent ? sidebar.theme.highlightColor : sidebar.theme.subtleBorderColor
            }

            Kirigami.Icon {
                source: subBtn.iconName
                implicitWidth: 14
                implicitHeight: 14
                color: subBtn.isCurrent ? sidebar.theme.highlightColor : (subHover.hovered ? sidebar.theme.textColor : sidebar.theme.mutedTextColor)
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: subBtn.label
                font.pointSize: sidebar.theme.fontBody * 0.92
                font.weight: subBtn.isCurrent ? Font.Medium : Font.Normal
                color: subBtn.isCurrent ? sidebar.theme.textColor : (subHover.hovered ? sidebar.theme.textColor : sidebar.theme.mutedTextColor)
                elide: Text.ElideRight
            }
        }

        HoverHandler { id: subHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: subBtn.clicked() }
    }
}
