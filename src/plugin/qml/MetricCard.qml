// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami

// Compact metric tile: channel label, large value, caption and a full-width
// trend line along the bottom edge. `available: false` shows a muted
// unavailable state instead of a misleading value.
PulseCard {
    id: metric

    property string title
    property string value
    property string unit
    property string caption
    property color channelColor: theme.cpuAccent
    property int severity: 0
    property bool showStatus: severity > 0
    property bool available: true
    property string unavailableText: KI18n.i18nd("loofi-pulse", "Unavailable")
    property var trend: []
    property real trendMin: NaN
    property real trendMax: NaN
    property bool dense: false

    interactive: true
    padding: dense ? theme.spacingM : theme.spacingL
    accentColor: theme.severityColor(severity)
    accentStrength: severity > 0 && severity < 4 ? (theme.isDark ? 0.10 : 0.07) : 0

    implicitWidth: Kirigami.Units.gridUnit * (dense ? 8 : 13)
    implicitHeight: body.implicitHeight + padding * 2 + trendLine.height

    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: available ? (value + " " + unit + (caption.length ? ", " + caption : "")) : unavailableText

    ColumnLayout {
        id: body
        width: parent.width
        spacing: metric.dense ? 2 : metric.theme.spacingXs

        RowLayout {
            Layout.fillWidth: true
            spacing: metric.theme.spacingS
            Rectangle {
                width: 8; height: 8; radius: 2.5
                color: metric.available ? metric.channelColor : metric.theme.disabledColor
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: metric.title
                elide: Text.ElideRight
                color: metric.theme.mutedTextColor
                font.pointSize: metric.dense ? metric.theme.fontCaption : metric.theme.fontBody
                font.weight: Font.Medium
            }
            StatusPill {
                visible: metric.showStatus && !metric.dense
                theme: metric.theme
                severity: metric.severity
                compact: true
                showDot: false
            }
        }

        RowLayout {
            spacing: 3
            QQC2.Label {
                text: metric.available ? metric.value : "—"
                color: metric.available ? metric.theme.textColor : metric.theme.faintTextColor
                font.pointSize: metric.dense ? metric.theme.fontHeadline : metric.theme.fontDisplay
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
                font.letterSpacing: -0.5
            }
            QQC2.Label {
                visible: metric.available && metric.unit.length > 0
                Layout.alignment: Qt.AlignBaseline
                text: metric.unit
                color: metric.theme.mutedTextColor
                font.pointSize: metric.dense ? metric.theme.fontCaption : metric.theme.fontBody
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            visible: !metric.dense
            text: metric.available ? metric.caption : metric.unavailableText
            elide: Text.ElideRight
            color: metric.theme.faintTextColor
            font.pointSize: metric.theme.fontCaption
        }
    }

    PulseSparkline {
        id: trendLine
        parent: metric
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        anchors.bottomMargin: 1
        height: metric.dense ? Kirigami.Units.gridUnit * 1.4 : Kirigami.Units.gridUnit * 2.4
        visible: metric.available
        values: metric.trend
        minValue: metric.trendMin
        maxValue: metric.trendMax
        color: metric.channelColor
        opacity: metric.hovered ? 1.0 : 0.85
    }
}
