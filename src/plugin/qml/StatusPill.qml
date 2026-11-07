// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Rounded status label. Either driven by `severity` (0–4) or by an
// explicit `text` and `color`.
Rectangle {
    id: pill

    property PulseTheme theme: PulseTheme {
        bgColor: pill.Kirigami.Theme.backgroundColor
        textColor: pill.Kirigami.Theme.textColor
        highlightColor: pill.Kirigami.Theme.highlightColor
        disabledColor: pill.Kirigami.Theme.disabledTextColor
        negativeColor: pill.Kirigami.Theme.negativeTextColor
        neutralColor: pill.Kirigami.Theme.neutralTextColor
        positiveColor: pill.Kirigami.Theme.positiveTextColor
    }
    property int severity: 0
    property string text: theme.severityShortLabel(severity)
    property color tone: theme.severityColor(severity)
    property bool showDot: true
    property bool compact: false

    implicitHeight: label.implicitHeight + (compact ? 2 : 6)
    implicitWidth: label.implicitWidth + (showDot ? dot.width + 5 : 0) + (compact ? 12 : 18)
    radius: height / 2
    color: theme.accentTint(tone, theme.isDark ? 0.18 : 0.13)

    Accessible.role: Accessible.StaticText
    Accessible.name: text


    Row {
        anchors.centerIn: parent
        spacing: 5
        Rectangle {
            id: dot
            visible: pill.showDot
            width: 6
            height: 6
            radius: 3
            anchors.verticalCenter: parent.verticalCenter
            color: pill.tone
        }
        QQC2.Label {
            id: label
            text: pill.text
            color: pill.tone
            font.pointSize: pill.theme.fontCaption
            font.weight: Font.DemiBold
            font.letterSpacing: 0.3
        }
    }
}
