// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

// Circular gauge. Two modes:
//  * value mode:     `value` 0..1 drawn as an arc in `color`
//  * segmented mode: `segments` is an array of colors, one equal arc each
// Center content goes in the default property.
Item {
    id: gauge

    property PulseTheme theme: PulseTheme {
        bgColor: gauge.Kirigami.Theme.backgroundColor
        textColor: gauge.Kirigami.Theme.textColor
        highlightColor: gauge.Kirigami.Theme.highlightColor
        disabledColor: gauge.Kirigami.Theme.disabledTextColor
        negativeColor: gauge.Kirigami.Theme.negativeTextColor
        neutralColor: gauge.Kirigami.Theme.neutralTextColor
        positiveColor: gauge.Kirigami.Theme.positiveTextColor
    }
    property real value: 0
    property color color: theme.highlightColor
    property var segments: []
    property real thickness: Math.max(3, Math.min(width, height) * 0.09)
    property real startAngle: -90
    property real sweep: 360
    property real segmentGap: 10
    property bool pulsing: false
    default property alias content: center.data

    readonly property real radius: Math.max(1, Math.min(width, height) / 2 - thickness / 2 - (pulsing ? 4 : 0))
    property real animatedValue: Math.max(0, Math.min(1, value))
    Behavior on animatedValue { NumberAnimation { duration: gauge.theme.durationLong; easing.type: Easing.OutCubic } }

    implicitWidth: Kirigami.Units.gridUnit * 6
    implicitHeight: implicitWidth

    // Warning halo; disabled with reduced motion.
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(gauge.width, gauge.height)
        height: width
        radius: width / 2
        color: "transparent"
        border.width: 2
        border.color: gauge.segments.length ? gauge.theme.neutralColor : gauge.color
        visible: gauge.pulsing && gauge.theme.motionAllowed
        opacity: 0
        SequentialAnimation on opacity {
            running: gauge.pulsing && gauge.theme.motionAllowed && gauge.visible
            loops: Animation.Infinite
            NumberAnimation { to: 0.45; duration: 900; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 0.05; duration: 900; easing.type: Easing.InOutQuad }
        }
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: gauge.segments.length ? "transparent" : gauge.theme.trackColor
            strokeWidth: gauge.thickness
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: gauge.width / 2; centerY: gauge.height / 2
                radiusX: gauge.radius; radiusY: gauge.radius
                startAngle: gauge.startAngle
                sweepAngle: gauge.sweep
            }
        }
        ShapePath {
            strokeColor: gauge.segments.length ? "transparent" : gauge.color
            strokeWidth: gauge.thickness
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: gauge.width / 2; centerY: gauge.height / 2
                radiusX: gauge.radius; radiusY: gauge.radius
                startAngle: gauge.startAngle
                sweepAngle: gauge.sweep * gauge.animatedValue
            }
        }
    }

    Repeater {
        model: gauge.segments
        delegate: Shape {
            id: seg
            required property var modelData
            required property int index
            readonly property real span: gauge.sweep / Math.max(1, gauge.segments.length)
            // Account for round caps so visual gaps stay constant.
            readonly property real capAngle: (gauge.thickness / 2) / gauge.radius * 180 / Math.PI
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: seg.modelData
                strokeWidth: gauge.thickness
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathAngleArc {
                    centerX: gauge.width / 2; centerY: gauge.height / 2
                    radiusX: gauge.radius; radiusY: gauge.radius
                    startAngle: gauge.startAngle + seg.index * seg.span + gauge.segmentGap / 2 + seg.capAngle
                    sweepAngle: Math.max(1, seg.span - gauge.segmentGap - seg.capAngle * 2)
                }
            }
        }
    }

    Item {
        id: center
        anchors.centerIn: parent
        width: gauge.radius * 2 - gauge.thickness
        height: width
    }
}
