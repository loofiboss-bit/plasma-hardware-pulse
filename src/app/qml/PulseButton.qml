pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

QQC2.Button {
    id: control

    property bool isPrimary: false
    property color accentColor: Kirigami.Theme.highlightColor

    PulseTheme {
        id: theme
        bgColor: control.Kirigami.Theme.backgroundColor
        disabledColor: control.Kirigami.Theme.disabledTextColor
        negativeColor: control.Kirigami.Theme.negativeTextColor
        neutralColor: control.Kirigami.Theme.neutralTextColor
        positiveColor: control.Kirigami.Theme.positiveTextColor
    }

    implicitHeight: Math.max(30, contentLayout.implicitHeight + 8)
    implicitWidth: contentLayout.implicitWidth + 20
    hoverEnabled: true

    contentItem: RowLayout {
        id: contentLayout
        spacing: 6
        Kirigami.Icon {
            source: control.icon.name
            visible: control.icon.name.length > 0
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: Kirigami.Units.iconSizes.small
            color: !control.enabled ? Kirigami.Theme.disabledTextColor
                   : control.isPrimary ? Kirigami.Theme.highlightedTextColor
                   : (control.checked ? control.accentColor : Kirigami.Theme.textColor)
        }
        QQC2.Label {
            text: control.text
            visible: control.text.length > 0
            font.bold: control.isPrimary || control.checked
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            color: !control.enabled ? Kirigami.Theme.disabledTextColor
                   : control.isPrimary ? Kirigami.Theme.highlightedTextColor
                   : (control.checked ? control.accentColor : Kirigami.Theme.textColor)
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        radius: theme.innerRadius
        color: !control.enabled ? theme.cardNestedBg
               : control.isPrimary
                 ? (control.down ? Qt.darker(control.accentColor, 1.25)
                    : (control.hovered ? Qt.lighter(control.accentColor, 1.1) : control.accentColor))
                 : (control.down ? theme.cardNestedBg
                    : (control.checked ? theme.accentTint(control.accentColor, theme.isDark ? 0.28 : 0.16)
                       : (control.hovered ? theme.elevatedSurfaceColor : theme.cardNestedBg)))
        border.width: 1
        border.color: !control.enabled ? theme.subtleBorderColor
                      : control.activeFocus ? Kirigami.Theme.focusColor
                      : control.isPrimary
                        ? Qt.darker(control.accentColor, 1.2)
                        : (control.checked ? control.accentColor
                           : (control.hovered ? theme.cardHoverBorderColor : theme.cardBorderColor))

        // Subtle top edge highlight for modern surface depth
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: 1
            radius: theme.innerRadius
            color: control.isPrimary ? Qt.rgba(1, 1, 1, 0.25) : (theme.isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.5))
            visible: control.enabled && !control.down
        }
    }
}
