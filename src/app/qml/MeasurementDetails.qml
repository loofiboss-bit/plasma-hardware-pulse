pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "MeasurementLabels.js" as MeasurementLabels

ColumnLayout {
    id: details
    required property var metadata
    property bool expanded: false
    Layout.fillWidth: true
    QQC2.Button {
        text: details.expanded ? "Hide measurement details" : "Show measurement details"
        icon.name: details.expanded ? "collapse-all" : "expand-all"
        checkable: true
        checked: details.expanded
        onClicked: details.expanded = checked
    }
    Repeater {
        model: details.expanded && details.metadata ? Object.keys(details.metadata) : []
        delegate: QQC2.Frame {
            id: entry
            required property string modelData
            readonly property var quality: details.metadata[modelData]
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                QQC2.Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: MeasurementLabels.metricLabel(entry.modelData) + " · " + MeasurementLabels.qualityLabel(entry.quality.state); font.bold: true }
                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "Source: " + (entry.quality.source || "Unavailable") + " · Sampled: "
                          + (Number(entry.quality.sampledAtMs) > 0 ? new Date(Number(entry.quality.sampledAtMs)).toLocaleTimeString() : "Not sampled")
                          + " · Reason: " + (entry.quality.detail || "No additional detail")
                }
            }
        }
    }
}
