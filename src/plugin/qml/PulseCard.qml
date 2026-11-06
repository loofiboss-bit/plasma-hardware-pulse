// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

// Elevated surface used by every Pulse card. Content is placed in the
// padded area; `interactive` enables hover lift and the `clicked` signal.
Item {
    id: card

    property PulseTheme theme: PulseTheme {
        bgColor: card.Kirigami.Theme.backgroundColor
        textColor: card.Kirigami.Theme.textColor
        highlightColor: card.Kirigami.Theme.highlightColor
        disabledColor: card.Kirigami.Theme.disabledTextColor
        negativeColor: card.Kirigami.Theme.negativeTextColor
        neutralColor: card.Kirigami.Theme.neutralTextColor
        positiveColor: card.Kirigami.Theme.positiveTextColor
    }
    property bool interactive: false
    property real padding: theme.spacingL
    property color surfaceColor: theme.cardColor
    property color accentColor: "transparent"
    property real accentStrength: 0
    readonly property bool hovered: hover.hovered
    default property alias content: contentArea.data

    signal clicked()

    implicitWidth: contentArea.childrenRect.width + padding * 2
    implicitHeight: contentArea.childrenRect.height + padding * 2
    activeFocusOnTab: interactive

    Kirigami.ShadowedRectangle {
        id: surface
        anchors.fill: parent
        anchors.topMargin: card.interactive && (card.hovered || card.activeFocus) ? -1 : 0
        anchors.bottomMargin: card.interactive && (card.hovered || card.activeFocus) ? 1 : 0
        radius: card.theme.cardRadius
        color: card.surfaceColor
        border.width: 1
        border.color: card.activeFocus ? Kirigami.Theme.focusColor
            : (card.interactive && card.hovered ? card.theme.cardHoverBorderColor : card.theme.cardBorderColor)
        shadow.size: card.interactive && card.hovered ? 18 : 10
        shadow.yOffset: card.interactive && card.hovered ? 4 : 2
        shadow.color: card.theme.cardShadowColor

        Behavior on anchors.topMargin { NumberAnimation { duration: card.theme.durationShort; easing.type: Easing.OutCubic } }
        Behavior on shadow.size { NumberAnimation { duration: card.theme.durationShort } }

        // Soft accent wash from the top-left corner (used for severity).
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: card.accentStrength > 0
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: card.theme.accentTint(card.accentColor, card.accentStrength) }
                GradientStop { position: 0.55; color: "transparent" }
            }
        }
    }

    Item {
        id: contentArea
        anchors.fill: parent
        anchors.margins: card.padding
        anchors.topMargin: card.padding + surface.anchors.topMargin
        anchors.bottomMargin: card.padding - surface.anchors.topMargin
    }

    HoverHandler {
        id: hover
        enabled: card.interactive
        cursorShape: card.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
    TapHandler {
        enabled: card.interactive
        onTapped: card.clicked()
    }
    Keys.onPressed: event => {
        if (card.interactive && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space)) {
            card.clicked()
            event.accepted = true
        }
    }
}
