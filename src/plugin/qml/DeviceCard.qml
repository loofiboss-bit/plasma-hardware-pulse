// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// DeviceCard: Reusable card for discovered hardware devices (fans, NVMe drives,
// GPUs, battery, network interfaces) with health status, primary readout, and progress bar.
PulseCard {
    id: card

    property string deviceName: ""
    property string deviceIcon: "hwinfo"
    property string statusText: ""
    property int statusSeverity: 0
    property string primaryValue: ""
    property string primaryUnit: ""
    property string secondaryText: ""
    property real progressValue: 0.0 // 0.0 to 1.0
    property bool showProgress: false
    property color progressColor: theme.channelColor("cpu")
    default property alias extraContent: extraColumn.data

    padding: theme.spacingM
    implicitWidth: Kirigami.Units.gridUnit * 12
    implicitHeight: layout.implicitHeight + padding * 2

    Accessible.role: Accessible.Pane
    Accessible.name: deviceName

    ColumnLayout {
        id: layout
        anchors.fill: parent
        spacing: card.theme.spacingS

        // Header: Device Icon, Name, and Status Pill
        RowLayout {
            Layout.fillWidth: true
            spacing: card.theme.spacingS

            Rectangle {
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                radius: 6
                color: card.theme.subtleSurfaceColor
                border.width: 1
                border.color: card.theme.subtleBorderColor

                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    source: card.deviceIcon
                    color: card.theme.mutedTextColor
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: card.deviceName
                font.pointSize: card.theme.fontBody
                font.weight: Font.DemiBold
                color: card.theme.textColor
                elide: Text.ElideRight
            }

            StatusPill {
                visible: card.statusText.length > 0
                theme: card.theme
                severity: card.statusSeverity
                text: card.statusText
                compact: true
                showDot: false
            }
        }

        // Primary readout (e.g. RPM, temperature, remaining capacity, throughput)
        RowLayout {
            spacing: 4
            visible: card.primaryValue.length > 0

            QQC2.Label {
                text: card.primaryValue
                font.pointSize: card.theme.fontHeadline
                font.weight: Font.Bold
                font.features: { "tnum": 1 }
                color: card.theme.textColor
            }

            QQC2.Label {
                visible: card.primaryUnit.length > 0
                Layout.alignment: Qt.AlignBaseline
                text: card.primaryUnit
                font.pointSize: card.theme.fontCaption
                color: card.theme.mutedTextColor
            }
        }

        // Optional Progress bar (e.g. Fan RPM % of max, Battery charge, NVMe spare capacity)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 6
            radius: 3
            color: card.theme.trackColor
            visible: card.showProgress

            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, card.progressValue))
                height: parent.height
                radius: 3
                color: card.progressColor

                Behavior on width {
                    NumberAnimation { duration: card.theme.durationLong; easing.type: Easing.OutCubic }
                }
            }
        }

        // Secondary descriptive info (e.g. Model, Bus, Driver, Warning flags)
        QQC2.Label {
            Layout.fillWidth: true
            text: card.secondaryText
            font.pointSize: card.theme.fontCaption
            color: card.theme.faintTextColor
            wrapMode: Text.WordWrap
            visible: card.secondaryText.length > 0
        }

        ColumnLayout {
            id: extraColumn
            Layout.fillWidth: true
            spacing: card.theme.spacingXs
        }
    }
}
