// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

// Clean Breeze surface used by Pulse components.
// Flat, responsive, without shadows, gradient washes, or hover-lift animations.
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
    property real padding: theme.spacingM
    property color surfaceColor: theme.cardColor
    property color accentColor: "transparent"
    property real accentStrength: 0
    readonly property bool hovered: hover.hovered
    default property alias content: contentArea.data

    signal clicked()

    implicitWidth: Math.max(contentArea.implicitWidth, contentArea.childrenRect.width) + padding * 2
    implicitHeight: {
        var h = 0
        for (var i = 0; i < contentArea.children.length; ++i) {
            var c = contentArea.children[i]
            var ch = (c.implicitHeight > 0) ? c.implicitHeight : c.height
            if (ch > h) h = ch
        }
        return Math.max(h, contentArea.childrenRect.height) + padding * 2
    }
    activeFocusOnTab: interactive

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: card.theme.innerRadius
        color: card.interactive && (card.hovered || card.activeFocus)
               ? card.theme.elevatedSurfaceColor
               : card.surfaceColor
        border.width: 1
        border.color: card.activeFocus
                      ? Kirigami.Theme.focusColor
                      : (card.interactive && card.hovered ? card.theme.cardHoverBorderColor : card.theme.cardBorderColor)
    }

    Item {
        id: contentArea
        anchors.fill: parent
        anchors.margins: card.padding
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
