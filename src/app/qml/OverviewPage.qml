pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    objectName: "overviewPage"
    title: "Overview"
    required property var controller
    property bool detailsExpanded: false

    function metric(available, value, suffix) {
        return available ? value + suffix : "Unavailable"
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Card {
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                Kirigami.Heading { text: page.controller.verdictHeadline; level: 2; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                QQC2.Label { text: page.controller.verdictCausalChain || "No contributing signal is currently available."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Kirigami.Theme.textColor }
                QQC2.Label { text: page.controller.verdictRecommendation || "Review the available measurements below."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Kirigami.Theme.textColor }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width < 720 ? 2 : 4
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing
            Repeater {
                model: [
                    { title: "CPU load", value: page.metric(page.controller.cpuLoadAvailable, page.controller.cpuLoadPercent.toFixed(1), "%") },
                    { title: "CPU temperature", value: page.metric(page.controller.cpuTempAvailable, page.controller.cpuPackageTemp.toFixed(1), " °C") },
                    { title: "Memory pressure", value: page.metric(page.controller.psiMemoryAvailable, page.controller.psiMemSome10.toFixed(1), "% time waiting") },
                    { title: "Package power", value: page.controller.packagePowerAvailable
                        ? page.controller.packageWatts.toFixed(1) + " W · " + (page.controller.packagePowerEstimated ? "Estimated" : "Measured")
                        : "Unavailable" }
                ]
                delegate: Kirigami.Card {
                    id: metricCard
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.minimumWidth: 140
                    contentItem: ColumnLayout {
                        QQC2.Label { text: metricCard.modelData.title; color: Kirigami.Theme.disabledTextColor }
                        Kirigami.Heading { text: metricCard.modelData.value; level: 3; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    }
                }
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            text: "Calculated pressure index: " + page.controller.responsivenessScore.toFixed(1)
                  + "%. 100 means little observed pressure; lower values mean more stalled work. This PSI-based index is not a measured response time."
            visible: page.controller.psiAvailable
        }

        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing
            QQC2.Button { text: "Open System Monitor"; icon.name: "utilities-system-monitor"; onClicked: page.controller.openSystemMonitor() }
            QQC2.Button { text: "Copy report"; icon.name: "edit-copy"; onClicked: page.controller.copyIncidentReportToClipboard() }
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: "Top process: " + (page.controller.culpritName || "Unavailable") + (page.controller.culpritName ? " · concurrent CPU load, not proof of cause" : "")
            color: Kirigami.Theme.textColor
            wrapMode: Text.WordWrap
        }

        QQC2.Button {
            objectName: "measurementDetailsButton"
            text: page.detailsExpanded ? "Hide measurement details" : "Show measurement details"
            checkable: true
            checked: page.detailsExpanded
            Accessible.name: text
            onClicked: page.detailsExpanded = checked
        }

        QQC2.GroupBox {
            visible: page.detailsExpanded
            Layout.fillWidth: true
            title: "Sensor and diagnostic details"
            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: Kirigami.Units.smallSpacing
                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing
                    QQC2.Label { text: "GPU load: " + page.metric(page.controller.gpuBusyAvailable, page.controller.gpuBusyPercent.toFixed(0), "%") }
                    QQC2.Label { text: "GPU temperature: " + page.metric(page.controller.gpuTempAvailable, page.controller.gpuTempCelsius.toFixed(0), " °C") }
                    QQC2.Label { text: "Fan: " + (page.controller.fanAvailable ? page.controller.primaryFanRpm + " RPM" : "Unavailable") }
                    QQC2.Label { text: "Thermal headroom: " + page.metric(page.controller.thermalHeadroomAvailable, page.controller.thermalHeadroom.toFixed(1), " °C") }
                    QQC2.Label { text: "Active power profile: " + (page.controller.activePowerProfile || "Unavailable") }
                }
                Kirigami.Heading { text: "Data quality and sources"; level: 3; Layout.fillWidth: true }
                Repeater {
                    model: [
                        { key: "cpuLoad", label: "CPU load" },
                        { key: "cpuTemperature", label: "CPU temperature" },
                        { key: "pressureIndex", label: "Pressure index" },
                        { key: "packagePower", label: "Package power" }
                    ]
                    delegate: QQC2.Label {
                        required property var modelData
                        readonly property var quality: page.controller.metricMetadata[modelData.key]
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: modelData.label + ": " + (quality
                              ? quality.state + " · " + quality.source + " · "
                                + (quality.sampledAtMs > 0 ? new Date(quality.sampledAtMs).toLocaleTimeString() : "Not sampled")
                                + (quality.detail ? " · " + quality.detail : "")
                              : "Unavailable")
                    }
                }
            }
        }
    }
}
