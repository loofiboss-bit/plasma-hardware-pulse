// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami

// PulsePageHeader: Standard header across all main views with title, subtitle,
// segmented time window selector (5 / 15 / 30 min) and action buttons.
Item {
    id: header

    property PulseTheme theme: PulseTheme {
        bgColor: header.Kirigami.Theme.backgroundColor
        textColor: header.Kirigami.Theme.textColor
        disabledColor: header.Kirigami.Theme.disabledTextColor
        negativeColor: header.Kirigami.Theme.negativeTextColor
        neutralColor: header.Kirigami.Theme.neutralTextColor
        positiveColor: header.Kirigami.Theme.positiveTextColor
        highlightColor: header.Kirigami.Theme.highlightColor
    }

    property string title: ""
    property string subtitle: ""
    property bool showTimeSelector: true
    property int selectedMinutes: 15
    property bool showSaveButton: false
    property bool showIncidentButton: false

    signal timeWindowSelected(int minutes)
    signal saveRequested()
    signal incidentRequested()

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight

    RowLayout {
        id: layout
        anchors.fill: parent
        spacing: header.theme.spacingM

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Kirigami.Heading {
                text: header.title
                level: 1
                font.pointSize: header.theme.fontHeadline
                font.weight: Font.Bold
                color: header.theme.textColor
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            QQC2.Label {
                text: header.subtitle
                font.pointSize: header.theme.fontCaption
                color: header.theme.mutedTextColor
                Layout.fillWidth: true
                elide: Text.ElideRight
                visible: header.subtitle.length > 0
            }
        }

        // Time window segmented switcher (5 min, 15 min, 30 min)
        Rectangle {
            visible: header.showTimeSelector
            radius: 8
            color: header.theme.cardNestedBg
            border.width: 1
            border.color: header.theme.subtleBorderColor
            implicitHeight: segRow.implicitHeight + 4
            implicitWidth: segRow.implicitWidth + 4

            RowLayout {
                id: segRow
                anchors.centerIn: parent
                spacing: 2

                Repeater {
                    model: [
                        { label: KI18n.i18nd("loofi-pulse", "5 min"), minutes: 5 },
                        { label: KI18n.i18nd("loofi-pulse", "15 min"), minutes: 15 },
                        { label: KI18n.i18nd("loofi-pulse", "30 min"), minutes: 30 }
                    ]

                    delegate: Rectangle {
                        id: segBtn
                        required property var modelData
                        readonly property bool isSelected: header.selectedMinutes === modelData.minutes
                        radius: 6
                        color: isSelected ? header.theme.elevatedSurfaceColor : (segHover.hovered ? header.theme.subtleSurfaceColor : "transparent")
                        border.width: isSelected ? 1 : 0
                        border.color: header.theme.cardBorderColor
                        implicitWidth: segLbl.implicitWidth + 16
                        implicitHeight: segLbl.implicitHeight + 8

                        QQC2.Label {
                            id: segLbl
                            anchors.centerIn: parent
                            text: segBtn.modelData.label
                            font.pointSize: header.theme.fontCaption
                            font.weight: segBtn.isSelected ? Font.Bold : Font.Normal
                            color: segBtn.isSelected ? header.theme.textColor : header.theme.mutedTextColor
                        }

                        HoverHandler { id: segHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: header.timeWindowSelected(segBtn.modelData.minutes) }
                    }
                }
            }
        }

        // Save Investigation Button
        QQC2.Button {
            visible: header.showSaveButton
            text: KI18n.i18nd("loofi-pulse", "Save Session")
            icon.name: "document-save"
            onClicked: header.saveRequested()
        }

        // Mark Incident Button
        QQC2.Button {
            visible: header.showIncidentButton
            text: KI18n.i18nd("loofi-pulse", "● Mark Incident")
            highlighted: true
            onClicked: header.incidentRequested()
        }
    }
}
