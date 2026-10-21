pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "MeasurementLabels.js" as MeasurementLabels

ColumnLayout {
    id: details
    required property var metadata
    property bool expanded: false
    Layout.fillWidth: true
    PulseButton {
        text: details.expanded ? KI18n.i18n("Hide measurement details") : KI18n.i18n("Show measurement details")
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
                    text: KI18n.i18n("Source: ") + (entry.quality.source || KI18n.i18n("Unavailable")) + KI18n.i18n(" · Sampled: ")
                          + (Number(entry.quality.sampledAtMs) > 0 ? new Date(Number(entry.quality.sampledAtMs)).toLocaleTimeString() : KI18n.i18n("Not sampled"))
                          + KI18n.i18n(" · Reason: ") + (entry.quality.detail || KI18n.i18n("No additional detail"))
                }
            }
        }
    }
}
