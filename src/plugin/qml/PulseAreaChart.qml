// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

// Larger time-series chart: grid lines, axis labels, one or more series and
// optional vertical markers (e.g. incidents) given as fractions 0..1.
//   series:  [{ values: [..], color: "#..", label: "CPU" }]
//   markers: [{ position: 0.42, color: "#..", label: "Incident" }]
Item {
    id: chart

    property PulseTheme theme: PulseTheme {
        bgColor: chart.Kirigami.Theme.backgroundColor
        textColor: chart.Kirigami.Theme.textColor
        highlightColor: chart.Kirigami.Theme.highlightColor
        disabledColor: chart.Kirigami.Theme.disabledTextColor
        negativeColor: chart.Kirigami.Theme.negativeTextColor
        neutralColor: chart.Kirigami.Theme.neutralTextColor
        positiveColor: chart.Kirigami.Theme.positiveTextColor
    }
    property var series: []
    property var markers: []
    property real minValue: 0
    property real maxValue: NaN
    property int gridLines: 3
    property var formatValue: function(v) { return Math.round(v).toString() }
    property string emptyText: ""
    property bool filled: true

    readonly property real computedMax: {
        if (isFinite(maxValue)) return maxValue
        var hi = minValue + 1
        for (var s = 0; s < (series || []).length; ++s) {
            var vals = series[s].values || []
            for (var i = 0; i < vals.length; ++i) {
                var v = Number(vals[i])
                if (isFinite(v) && v > hi) hi = v
            }
        }
        return hi * 1.08
    }
    readonly property bool hasData: {
        for (var s = 0; s < (series || []).length; ++s)
            if ((series[s].values || []).length > 1) return true
        return false
    }
    readonly property real axisWidth: axisMetrics.advanceWidth + theme.spacingS

    implicitHeight: Kirigami.Units.gridUnit * 10

    TextMetrics {
        id: axisMetrics
        font.pointSize: chart.theme.fontCaption
        text: chart.formatValue(chart.computedMax)
    }

    Item {
        id: plot
        anchors.fill: parent
        anchors.leftMargin: chart.axisWidth
        anchors.bottomMargin: 2

        Repeater {
            model: chart.gridLines + 1
            delegate: Item {
                id: gridRow
                required property int index
                readonly property real fraction: index / chart.gridLines
                width: plot.width
                y: plot.height * (1 - fraction)
                Rectangle {
                    width: parent.width
                    height: 1
                    color: chart.theme.subtleBorderColor
                }
                QQC2.Label {
                    x: -chart.axisWidth
                    y: -height / 2
                    width: chart.axisWidth - chart.theme.spacingS
                    horizontalAlignment: Text.AlignRight
                    text: chart.formatValue(chart.minValue + gridRow.fraction * (chart.computedMax - chart.minValue))
                    font.pointSize: chart.theme.fontCaption
                    color: chart.theme.faintTextColor
                }
            }
        }

        Repeater {
            model: chart.series
            delegate: PulseSparkline {
                required property var modelData
                anchors.fill: parent
                values: modelData.values || []
                color: modelData.color || chart.theme.cpuAccent
                minValue: chart.minValue
                maxValue: chart.computedMax
                filled: chart.filled && chart.series.length === 1
                lineWidth: 2
                topPadding: 0
            }
        }

        Repeater {
            model: chart.markers
            delegate: Item {
                id: marker
                required property var modelData
                x: Math.max(0, Math.min(1, modelData.position)) * plot.width
                height: plot.height
                Rectangle {
                    width: 2
                    height: parent.height
                    x: -1
                    color: marker.modelData.color || chart.theme.neutralColor
                    opacity: 0.8
                }
                Rectangle {
                    width: 8; height: 8; radius: 4
                    x: -4; y: -2
                    color: marker.modelData.color || chart.theme.neutralColor
                }
                QQC2.ToolTip.text: marker.modelData.label || ""
                QQC2.ToolTip.visible: (marker.modelData.label || "").length > 0 && markerHover.hovered
                HoverHandler { id: markerHover }
            }
        }
    }

    QQC2.Label {
        anchors.centerIn: plot
        visible: !chart.hasData && chart.emptyText.length > 0
        text: chart.emptyText
        color: chart.theme.faintTextColor
    }
}
