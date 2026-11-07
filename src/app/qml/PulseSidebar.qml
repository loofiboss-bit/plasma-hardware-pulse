// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

// Fixed navigation sidebar matching the requested design
Rectangle {
    id: sidebar

    required property PulseTheme theme
    required property var controller
    property string currentPage: "overview"

    signal pageSelected(string pageId, string section)

    width: 210
    color: "#0e131b"

    // Subtle right border
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: "#1a222e"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        // Navigation Items List
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            NavButton {
                pageId: "overview"
                label: KI18n.i18n("Översikt")
                iconName: "go-home"
                isCurrent: sidebar.currentPage === "overview"
                onClicked: sidebar.pageSelected("overview", "")
            }

            NavButton {
                pageId: "pressure"
                label: KI18n.i18n("Prestanda")
                iconName: "speedometer"
                isCurrent: sidebar.currentPage === "pressure"
                onClicked: sidebar.pageSelected("pressure", "overview")
            }

            NavButton {
                pageId: "power"
                label: KI18n.i18n("Effekt & Kylning")
                iconName: "battery-profile-performance"
                isCurrent: sidebar.currentPage === "power"
                onClicked: sidebar.pageSelected("power", "cpu")
            }

            NavButton {
                pageId: "gpu"
                label: KI18n.i18n("Grafik (GPU)")
                iconName: "monitor-symbolic"
                isCurrent: sidebar.currentPage === "power" && false
                onClicked: sidebar.pageSelected("power", "gpu")
            }

            NavButton {
                pageId: "battery"
                label: KI18n.i18n("Batteri")
                iconName: "battery-080"
                isCurrent: false
                onClicked: sidebar.pageSelected("power", "battery")
            }

            NavButton {
                pageId: "sensors"
                label: KI18n.i18n("Sensorer")
                iconName: "temperature-normal-symbolic"
                isCurrent: false
                onClicked: sidebar.pageSelected("power", "sensors")
            }

            NavButton {
                pageId: "flight"
                label: KI18n.i18n("Historik & Logg")
                iconName: "chronometer"
                isCurrent: sidebar.currentPage === "flight"
                onClicked: sidebar.pageSelected("flight", "")
            }

            NavButton {
                pageId: "profile"
                label: KI18n.i18n("Profiler")
                iconName: "configure-shortcuts"
                isCurrent: false
                onClicked: sidebar.pageSelected("power", "profile")
            }

            NavButton {
                pageId: "settings"
                label: KI18n.i18n("Inställningar")
                iconName: "settings-configure"
                isCurrent: sidebar.currentPage === "settings"
                onClicked: sidebar.pageSelected("settings", "")
            }
        }

        Item {
            Layout.fillHeight: true
        }

        // Host System Info Badge
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 4
            radius: 8
            color: "#141a24"
            border.width: 1
            border.color: "#212a39"
            implicitHeight: sysInfoRow.implicitHeight + 16

            RowLayout {
                id: sysInfoRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 10

                // Circular Fedora badge
                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: 16
                    color: "#294172"

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        implicitWidth: 20
                        implicitHeight: 20
                        source: "fedora-logo-icon"
                        fallback: "distributor-logo-fedora"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    QQC2.Label {
                        text: "Fedora Linux 45"
                        font.bold: true
                        font.pointSize: 9
                        color: "#ffffff"
                    }

                    QQC2.Label {
                        text: "KDE Plasma 6.7.5"
                        font.pointSize: 8
                        color: "#94a3b8"
                    }

                    QQC2.Label {
                        text: "Kärna: 7.2.15"
                        font.pointSize: 8
                        color: "#94a3b8"
                    }

                    QQC2.Label {
                        text: "HP EliteBook 840 G8"
                        font.pointSize: 8
                        color: "#94a3b8"
                        elide: Text.ElideRight
                        Layout.fillWidth: true
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
        Layout.preferredHeight: 38
        radius: 8
        color: isCurrent ? "#2563eb" : (btnHover.hovered ? "#192230" : "transparent")

        Accessible.role: Accessible.Button
        Accessible.name: label

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            Kirigami.Icon {
                source: btn.iconName
                implicitWidth: 16
                implicitHeight: 16
                color: btn.isCurrent ? "#ffffff" : (btnHover.hovered ? "#e2e8f0" : "#64748b")
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: btn.label
                font.pointSize: 9.5
                font.weight: btn.isCurrent ? Font.Bold : Font.Normal
                color: btn.isCurrent ? "#ffffff" : (btnHover.hovered ? "#f1f5f9" : "#94a3b8")
                elide: Text.ElideRight
            }
        }

        HoverHandler { id: btnHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: btn.clicked() }
    }
}
