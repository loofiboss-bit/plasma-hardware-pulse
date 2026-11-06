// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// FindingRow: Displays a diagnostic observation or confirmed problem,
// sorted by severity, with rule identifier, evidence snippet, and clickable action.
Item {
    id: row

    property PulseTheme theme: PulseTheme {
        bgColor: row.Kirigami.Theme.backgroundColor
        textColor: row.Kirigami.Theme.textColor
        disabledColor: row.Kirigami.Theme.disabledTextColor
        negativeColor: row.Kirigami.Theme.negativeTextColor
        neutralColor: row.Kirigami.Theme.neutralTextColor
        positiveColor: row.Kirigami.Theme.positiveTextColor
        highlightColor: row.Kirigami.Theme.highlightColor
    }
    property int severity: 1
    property string ruleId: ""
    property string title: ""
    property string evidence: ""
    property string actionText: ""
    property string actionPage: ""
    property string actionSection: ""

    signal actionTriggered(string targetPage, string targetSection)

    implicitWidth: layout.implicitWidth
    implicitHeight: Math.max(48, layout.implicitHeight + theme.spacingS * 2)

    Accessible.role: Accessible.ListItem
    Accessible.name: title
    Accessible.description: evidence

    Rectangle {
        id: bg
        anchors.fill: parent
        color: hoverHandler.hovered ? row.theme.tableRowHoverBg : "transparent"
        radius: row.theme.innerRadius

        HoverHandler {
            id: hoverHandler
        }
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.leftMargin: row.theme.spacingM
        anchors.rightMargin: row.theme.spacingM
        spacing: row.theme.spacingM

        // Severity indicator badge
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            radius: 8
            color: row.theme.severityBgColor(row.severity)
            border.width: 1
            border.color: row.theme.severityBorderColor(row.severity)

            Kirigami.Icon {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: row.theme.severityIcon(row.severity)
                color: row.theme.severityColor(row.severity)
            }
        }

        // Title and Evidence details
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            QQC2.Label {
                Layout.fillWidth: true
                text: row.title
                font.pointSize: row.theme.fontBody
                font.weight: Font.DemiBold
                color: row.theme.textColor
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                QQC2.Label {
                    visible: row.ruleId.length > 0
                    text: row.ruleId
                    font.pointSize: row.theme.fontCaption
                    font.weight: Font.Medium
                    color: row.theme.channelColor("cpu")
                }

                QQC2.Label {
                    visible: row.ruleId.length > 0 && row.evidence.length > 0
                    text: "·"
                    color: row.theme.faintTextColor
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    text: row.evidence
                    font.pointSize: row.theme.fontCaption
                    color: row.theme.mutedTextColor
                    elide: Text.ElideRight
                }
            }
        }

        // Direct action button / link
        QQC2.Button {
            visible: row.actionText.length > 0
            text: row.actionText
            flat: true
            font.pointSize: row.theme.fontCaption
            font.weight: Font.DemiBold
            Accessible.name: row.actionText

            contentItem: QQC2.Label {
                text: parent.text
                color: parent.hovered ? row.theme.highlightColor : row.theme.mutedTextColor
                font: parent.font
            }

            onClicked: row.actionTriggered(row.actionPage, row.actionSection)
        }
    }
}
