// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes

// Lightweight trend line with optional gradient fill.
// `values` is a plain array of numbers; non-finite entries are skipped.
Item {
    id: spark

    property var values: []
    property real minValue: NaN
    property real maxValue: NaN
    property color color: "#3daee9"
    property bool filled: true
    property real lineWidth: 1.6
    property real fillOpacity: 0.26
    property real topPadding: lineWidth + 1

    readonly property var finiteValues: {
        var out = []
        var src = values || []
        for (var i = 0; i < src.length; ++i) {
            var v = Number(src[i])
            if (isFinite(v)) out.push(v)
        }
        return out
    }
    readonly property real effectiveMin: {
        if (isFinite(minValue)) return minValue
        return finiteValues.length ? Math.min.apply(null, finiteValues) : 0
    }
    readonly property real effectiveMax: {
        var hi = isFinite(maxValue) ? maxValue : (finiteValues.length ? Math.max.apply(null, finiteValues) : 1)
        return hi > effectiveMin ? hi : effectiveMin + 1
    }
    readonly property var points: {
        var n = finiteValues.length
        if (n < 2 || width <= 0 || height <= 0) return []
        var pts = []
        var range = effectiveMax - effectiveMin
        var usable = height - topPadding - lineWidth / 2
        for (var i = 0; i < n; ++i) {
            var t = (Math.min(effectiveMax, Math.max(effectiveMin, finiteValues[i])) - effectiveMin) / range
            pts.push(Qt.point(i * width / (n - 1), topPadding + (1 - t) * usable))
        }
        return pts
    }
    readonly property var areaPoints: points.length
        ? points.concat([Qt.point(width, height), Qt.point(0, height)]) : []

    Accessible.ignored: true

    Shape {
        anchors.fill: parent
        visible: spark.points.length > 1
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: 0; y2: spark.height
                GradientStop { position: 0; color: Qt.rgba(spark.color.r, spark.color.g, spark.color.b, spark.filled ? spark.fillOpacity : 0) }
                GradientStop { position: 1; color: Qt.rgba(spark.color.r, spark.color.g, spark.color.b, 0) }
            }
            PathPolyline { path: spark.areaPoints }
        }
        ShapePath {
            strokeColor: spark.color
            strokeWidth: spark.lineWidth
            fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin
            capStyle: ShapePath.RoundCap
            PathPolyline { path: spark.points }
        }
    }
}
