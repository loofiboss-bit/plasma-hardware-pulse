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
    signal navigateRequested(string pageName)

    function metric(available, value, suffix) {
        return available ? value + suffix : "Unavailable"
    }

    function severityColor(sev) {
        if (sev === 4) return Kirigami.Theme.disabledTextColor
        if (sev === 3) return Kirigami.Theme.negativeTextColor
        if (sev === 2) return Kirigami.Theme.negativeTextColor
        if (sev === 1) return Kirigami.Theme.neutralTextColor
        return Kirigami.Theme.positiveTextColor
    }

    function severityBadgeText(sev) {
        if (sev === 4) return "WARMING UP"
        if (sev === 3) return "THROTTLING"
        if (sev === 2) return "BOTTLENECK"
        if (sev === 1) return "ELEVATED"
        return "NOMINAL"
    }

    function targetPageForDomain(domain) {
        if (!domain) return "power"
        if (domain.indexOf("Memory") >= 0 || domain.indexOf("Storage") >= 0) return "pressure"
        if (domain.indexOf("CPU") >= 0 || domain.indexOf("Power") >= 0 || domain.indexOf("Graphics") >= 0) return "power"
        return "flight"
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: verdictCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: 10
            color: Kirigami.Theme.backgroundColor
            border.width: page.controller.verdictSeverity >= 2 ? 2.5 : 2
            border.color: page.severityColor(page.controller.verdictSeverity)

            ColumnLayout {
                id: verdictCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Rectangle {
                        Layout.preferredWidth: 12
                        Layout.preferredHeight: 12
                        radius: 6
                        color: page.severityColor(page.controller.verdictSeverity)
                    }

                    Rectangle {
                        radius: 4
                        color: Kirigami.Theme.alternateBackgroundColor
                        border.width: 1
                        border.color: page.severityColor(page.controller.verdictSeverity)
                        implicitWidth: sevLabel.implicitWidth + 12
                        implicitHeight: sevLabel.implicitHeight + 4
                        QQC2.Label {
                            id: sevLabel
                            anchors.centerIn: parent
                            text: page.severityBadgeText(page.controller.verdictSeverity)
                            color: page.severityColor(page.controller.verdictSeverity)
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }

                    Kirigami.Heading {
                        text: page.controller.verdictHeadline
                        level: 2
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    QQC2.Label {
                        text: page.controller.primaryBottleneckDomain || "System"
                        color: Kirigami.Theme.disabledTextColor
                        font.bold: true
                    }
                }

                QQC2.Label {
                    text: page.controller.verdictCausalChain || "No contributing signal is currently available."
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.textColor
                }

                QQC2.Label {
                    text: "Next step: " + (page.controller.verdictRecommendation || "Review the available measurements below.")
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.disabledTextColor
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Button {
                        icon.name: "go-next"
                        text: page.controller.verdictSeverity > 0 && page.controller.verdictSeverity < 4
                              ? "Inspect " + (page.controller.primaryBottleneckDomain || "details")
                              : "Power & Cooling"
                        onClicked: page.navigateRequested(page.targetPageForDomain(page.controller.primaryBottleneckDomain))
                    }

                    QQC2.Button {
                        icon.name: "chronometer"
                        text: "Open History"
                        onClicked: page.navigateRequested("flight")
                    }

                    QQC2.Button {
                        visible: page.controller.powerProfileServiceAvailable
                                 && page.controller.activePowerProfile === "power-saver"
                                 && page.controller.verdictSeverity >= 1
                                 && page.controller.availablePowerProfiles.indexOf("balanced") >= 0
                        icon.name: "speedometer"
                        text: "Switch to Balanced profile"
                        enabled: !page.controller.powerProfileChangePending
                        onClicked: page.controller.setPowerProfile("balanced")
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width < 720 ? 2 : 3
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing
            Repeater {
                model: [
                    {
                        title: "CPU load",
                        value: page.metric(page.controller.cpuLoadAvailable, page.controller.cpuLoadPercent.toFixed(1), "%"),
                        subtitle: page.controller.cpuAvgFreqMhz > 0
                            ? page.controller.cpuAvgFreqMhz + " MHz avg · " + page.controller.cpuPeakFreqMhz + " MHz peak"
                            : "Frequency unavailable",
                        ratio: page.controller.cpuLoadAvailable ? Math.min(1.0, Math.max(0.0, page.controller.cpuLoadPercent / 100.0)) : 0.0,
                        barAvailable: page.controller.cpuLoadAvailable,
                        targetPage: "power"
                    },
                    {
                        title: "CPU temperature",
                        value: page.metric(page.controller.cpuTempAvailable, page.controller.cpuPackageTemp.toFixed(1), " °C"),
                        subtitle: page.controller.thermalHeadroomAvailable
                            ? page.controller.thermalHeadroom.toFixed(1) + " °C headroom"
                            : "Thermal limit unavailable",
                        ratio: page.controller.cpuTempAvailable ? Math.min(1.0, Math.max(0.0, (page.controller.cpuPackageTemp - 25.0) / 75.0)) : 0.0,
                        barAvailable: page.controller.cpuTempAvailable,
                        targetPage: "power"
                    },
                    {
                        title: "Graphics (GPU)",
                        value: page.controller.gpuBusyAvailable
                            ? page.controller.gpuBusyPercent.toFixed(0) + "% load"
                            : (page.controller.gpuFrequencyAvailable ? page.controller.gpuCurrentFreqMhz + " MHz" : "Unavailable"),
                        subtitle: (page.controller.gpuTempAvailable ? page.controller.gpuTempCelsius.toFixed(0) + " °C" : "Temp N/A")
                            + (page.controller.gpuFrequencyAvailable ? " · " + page.controller.gpuCurrentFreqMhz + " MHz" : "")
                            + (page.controller.gpuVramAvailable ? " · " + Math.round(page.controller.gpuVramUsedMib) + " MiB VRAM" : ""),
                        ratio: page.controller.gpuBusyAvailable
                            ? Math.min(1.0, Math.max(0.0, page.controller.gpuBusyPercent / 100.0))
                            : (page.controller.gpuFrequencyAvailable && page.controller.gpuBoostFreqMhz > 0
                               ? Math.min(1.0, Math.max(0.0, page.controller.gpuCurrentFreqMhz / page.controller.gpuBoostFreqMhz)) : 0.0),
                        barAvailable: page.controller.gpuBusyAvailable || (page.controller.gpuFrequencyAvailable && page.controller.gpuBoostFreqMhz > 0),
                        targetPage: "power"
                    },
                    {
                        title: "Memory pressure",
                        value: page.metric(page.controller.psiMemoryAvailable, page.controller.psiMemSome10.toFixed(1), "% time waiting"),
                        subtitle: page.controller.memoryAvailable
                            ? "RAM: " + page.controller.ramUsedGib.toFixed(1) + " / " + page.controller.ramTotalGib.toFixed(1) + " GiB"
                              + (page.controller.zramCompressionAvailable ? " · ZRAM " + page.controller.zramCompressionRatio.toFixed(1) + "×" : "")
                            : "RAM reading unavailable",
                        ratio: page.controller.psiMemoryAvailable ? Math.min(1.0, Math.max(0.0, page.controller.psiMemSome10 / 25.0)) : 0.0,
                        barAvailable: page.controller.psiMemoryAvailable,
                        targetPage: "pressure"
                    },
                    {
                        title: "Storage & disk I/O",
                        value: page.controller.diskThroughputAvailable
                            ? Number(page.controller.diskReadMibPerSec + page.controller.diskWriteMibPerSec).toFixed(1) + " MiB/s"
                            : page.metric(page.controller.psiIoFullAvailable, page.controller.psiIoFull10.toFixed(1), "% I/O wait"),
                        subtitle: (page.controller.diskThroughputAvailable
                            ? "R " + page.controller.diskReadMibPerSec.toFixed(1) + " · W " + page.controller.diskWriteMibPerSec.toFixed(1) + " MiB/s · "
                            : "") + (page.controller.nvmeTempAvailable ? "NVMe " + page.controller.nvmeTempCelsius.toFixed(0) + " °C" : "NVMe temp N/A"),
                        ratio: page.controller.diskThroughputAvailable
                            ? Math.min(1.0, Math.max(0.0, (page.controller.diskReadMibPerSec + page.controller.diskWriteMibPerSec) / 500.0))
                            : (page.controller.psiIoFullAvailable ? Math.min(1.0, Math.max(0.0, page.controller.psiIoFull10 / 25.0)) : 0.0),
                        barAvailable: page.controller.diskThroughputAvailable || page.controller.psiIoFullAvailable,
                        targetPage: "pressure"
                    },
                    {
                        title: "Package power",
                        value: page.controller.packagePowerAvailable
                            ? page.controller.packageWatts.toFixed(1) + " W · " + (page.controller.packagePowerEstimated ? "Estimated" : "Measured")
                            : "Unavailable",
                        subtitle: "Profile: " + (page.controller.activePowerProfile || "Unavailable")
                            + (page.controller.fanAvailable ? " · " + page.controller.primaryFanRpm + " RPM" : ""),
                        ratio: page.controller.packagePowerAvailable
                            ? Math.min(1.0, Math.max(0.0, page.controller.packageWatts / Math.max(25.0, page.controller.pl1LimitWatts > 0 ? page.controller.pl1LimitWatts : 45.0)))
                            : 0.0,
                        barAvailable: page.controller.packagePowerAvailable,
                        targetPage: "power"
                    }
                ]
                delegate: Kirigami.Card {
                    id: metricCard
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.minimumWidth: 140
                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing
                        RowLayout {
                            Layout.fillWidth: true
                            QQC2.Label {
                                text: metricCard.modelData.title
                                color: Kirigami.Theme.disabledTextColor
                                font.bold: true
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            QQC2.ToolButton {
                                icon.name: "go-next"
                                display: QQC2.AbstractButton.IconOnly
                                Accessible.name: "Open " + metricCard.modelData.title + " details"
                                onClicked: page.navigateRequested(metricCard.modelData.targetPage)
                            }
                        }
                        Kirigami.Heading {
                            text: metricCard.modelData.value
                            level: 3
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            radius: 3
                            color: Kirigami.Theme.backgroundColor
                            Rectangle {
                                visible: metricCard.modelData.barAvailable
                                width: parent.width * metricCard.modelData.ratio
                                height: parent.height
                                radius: 3
                                color: metricCard.modelData.ratio > 0.85 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.highlightColor
                            }
                        }
                        QQC2.Label {
                            text: metricCard.modelData.subtitle
                            color: Kirigami.Theme.disabledTextColor
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
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
            QQC2.Button { text: "Save report + JSON"; icon.name: "document-save"; onClicked: page.controller.exportIncidentReportToFile() }
            QQC2.Button {
                visible: page.controller.lastActionPath.length > 0
                text: "Open reports folder"
                icon.name: "folder-open"
                onClicked: page.controller.openReportsFolder()
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: "Top process: " + (page.controller.culpritName
                  ? page.controller.culpritName + " (" + page.controller.culpritCpuPercent.toFixed(1) + "% CPU, " + Math.round(page.controller.culpritRssMb) + " MiB RSS) · concurrent CPU load, not proof of cause"
                  : "Unavailable")
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
                    QQC2.Label { text: "GPU driver: " + (page.controller.gpuAvailable && page.controller.gpuDriverName ? page.controller.gpuDriverName : "Unavailable") }
                    QQC2.Label { text: "Fan: " + (page.controller.fanAvailable ? page.controller.primaryFanRpm + " RPM" : "Unavailable") }
                    QQC2.Label { text: "Thermal headroom: " + page.metric(page.controller.thermalHeadroomAvailable, page.controller.thermalHeadroom.toFixed(1), " °C") }
                    QQC2.Label { text: "Active power profile: " + (page.controller.activePowerProfile || "Unavailable") }
                }
                Kirigami.Heading { text: "Data quality and sources"; level: 3; Layout.fillWidth: true }
                Repeater {
                    model: [
                        { key: "cpuLoad", label: "CPU load" },
                        { key: "cpuTemperature", label: "CPU temperature" },
                        { key: "gpuLoad", label: "GPU load" },
                        { key: "pressureIndex", label: "Pressure index" },
                        { key: "diskThroughput", label: "Disk throughput" },
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
