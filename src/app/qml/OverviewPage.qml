pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "MeasurementLabels.js" as MeasurementLabels
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    objectName: "overviewPage"
    title: KI18n.i18n("Overview")
    titleDelegate: Item {
        width: parent ? parent.width : 0
        height: 0
        visible: false
    }
    required property var controller

    function focusSection(section) {
        if (!section) return true
        var targets = {"status": verdictHeading, "measurements": detailsCol}
        var target = targets[section]
        if (!target) return false
        if (section === "measurements") page.detailsExpanded = true
        if (section === "status") page.whyExpanded = true
        Qt.callLater(function() {
            var position = target.mapToItem(page.flickable.contentItem, 0, 0)
            page.flickable.contentY = Math.max(0, Math.min(position.y - 12,
                page.flickable.contentHeight - page.flickable.height))
            target.forceActiveFocus(Qt.TabFocusReason)
        })
        return true
    }

    property bool detailsExpanded: false
    property bool whyExpanded: false
    signal navigateRequested(string pageName, string section)

    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: page.Kirigami.Theme.backgroundColor
        disabledColor: page.Kirigami.Theme.disabledTextColor
        negativeColor: page.Kirigami.Theme.negativeTextColor
        neutralColor: page.Kirigami.Theme.neutralTextColor
        positiveColor: page.Kirigami.Theme.positiveTextColor
    }

    function metric(available, value, suffix) {
        return available ? value + suffix : KI18n.i18n("Unavailable")
    }

    function temp(available, celsius) {
        return available ? MeasurementLabels.formatTemperature(celsius, page.controller.temperatureUnit) : KI18n.i18n("Unavailable")
    }

    function sectionForDomain(domain) {
        var d = String(domain || "").toLowerCase()
        if (d.indexOf("memory") >= 0) return "memory"
        if (d.indexOf("storage") >= 0) return "storage"
        if (d.indexOf("graphics") >= 0) return "gpu"
        if (d.indexOf("battery") >= 0) return "battery"
        if (d.indexOf("power") >= 0) return "profile"
        return "cpu"
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

        // 1. Hero Causal Verdict & Responsiveness Banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: verdictMainRow.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: page.controller.verdictSeverity >= 2 ? 2 : 1.5
            border.color: theme.severityBorderColor(page.controller.verdictSeverity)

            // Subtle severity tint overlay at top-left
            Rectangle {
                anchors.fill: parent
                radius: theme.cardRadius
                color: theme.severityBgColor(page.controller.verdictSeverity)
                opacity: 0.55
            }

            RowLayout {
                id: verdictMainRow
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            Layout.preferredWidth: 12
                            Layout.preferredHeight: 12
                            radius: 6
                            color: theme.severityColor(page.controller.verdictSeverity)
                        }

                        Rectangle {
                            radius: theme.pillRadius
                            color: theme.severityBgColor(page.controller.verdictSeverity)
                            border.width: 1
                            border.color: theme.severityColor(page.controller.verdictSeverity)
                            implicitWidth: sevLabel.implicitWidth + 14
                            implicitHeight: sevLabel.implicitHeight + 6
                            QQC2.Label {
                                id: sevLabel
                                anchors.centerIn: parent
                                text: theme.severityBadgeText(page.controller.verdictSeverity)
                                color: theme.severityColor(page.controller.verdictSeverity)
                                font.bold: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                        }

                        Rectangle {
                            radius: theme.pillRadius
                            color: theme.subtleSurfaceColor
                            border.width: 1
                            border.color: theme.subtleBorderColor
                            implicitWidth: domLabel.implicitWidth + 12
                            implicitHeight: domLabel.implicitHeight + 6
                            QQC2.Label {
                                id: domLabel
                                anchors.centerIn: parent
                                text: page.controller.primaryBottleneckDomain || "System"
                                color: Kirigami.Theme.disabledTextColor
                                font.bold: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    Kirigami.Heading {
                        id: verdictHeading
                        objectName: "section_status"
                        focus: true
                        text: KI18n.i18n(page.controller.verdictHeadline)
                        level: 2
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    QQC2.Label {
                        text: KI18n.i18n("Original technical evidence (preserved language):") + "\n" + (page.controller.verdictCausalChain || KI18n.i18n("No contributing signal is currently available."))
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.textColor
                    }

                    PulseButton {
                        objectName: "whyStatusButton"
                        text: page.whyExpanded ? KI18n.i18n("Hide status explanation") : KI18n.i18n("Why this status?")
                        icon.name: page.whyExpanded ? "collapse-all" : "help-about"
                        checkable: true
                        checked: page.whyExpanded
                        onClicked: page.whyExpanded = checked
                    }

                    Rectangle {
                        visible: page.whyExpanded
                        Layout.fillWidth: true
                        implicitHeight: whyInnerCol.implicitHeight + 20
                        radius: theme.innerRadius
                        color: theme.cardNestedBg
                        border.width: 1
                        border.color: theme.subtleBorderColor

                        ColumnLayout {
                            id: whyInnerCol
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Rectangle {
                                    radius: theme.pillRadius
                                    color: theme.subtleSurfaceColor
                                    border.width: 1
                                    border.color: theme.subtleBorderColor
                                    implicitWidth: ruleBadgeLbl.implicitWidth + 12
                                    implicitHeight: ruleBadgeLbl.implicitHeight + 4
                                    QQC2.Label {
                                        id: ruleBadgeLbl
                                        anchors.centerIn: parent
                                        text: KI18n.i18n("Rule: ") + page.controller.verdictRuleId
                                        font.bold: true
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        color: Kirigami.Theme.textColor
                                    }
                                }

                                QQC2.Label {
                                    text: KI18n.i18n("Observed for ") + page.controller.verdictDurationSeconds.toFixed(1) + KI18n.i18n(" s")
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }

                            Repeater {
                                model: page.controller.verdictEvidence
                                delegate: RowLayout {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Kirigami.Icon {
                                        source: "dialog-ok"
                                        Layout.preferredWidth: 12
                                        Layout.preferredHeight: 12
                                        color: Kirigami.Theme.positiveTextColor
                                    }
                                    QQC2.Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        text: parent.modelData
                                    }
                                }
                            }

                            Repeater {
                                model: page.controller.verdictMissingEvidence
                                delegate: RowLayout {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Kirigami.Icon {
                                        source: "dialog-information"
                                        Layout.preferredWidth: 12
                                        Layout.preferredHeight: 12
                                        color: Kirigami.Theme.disabledTextColor
                                    }
                                    QQC2.Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        color: Kirigami.Theme.disabledTextColor
                                        text: KI18n.i18n("Missing: ") + parent.modelData
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: whyRecLbl.implicitHeight + 8
                                radius: 4
                                color: theme.subtleSurfaceColor
                                QQC2.Label {
                                    id: whyRecLbl
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    font.bold: true
                                    color: Kirigami.Theme.textColor
                                    text: KI18n.i18n("Recommendation: ") + KI18n.i18n(page.controller.verdictRecommendation)
                                }
                            }
                        }
                    }

                    Flow {
                        visible: page.controller.verdictContributingSignals.length > 0
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Repeater {
                            model: page.controller.verdictContributingSignals
                            delegate: Rectangle {
                                id: contribPill
                                required property string modelData
                                radius: theme.pillRadius
                                color: theme.subtleSurfaceColor
                                border.width: 1
                                border.color: theme.subtleBorderColor
                                implicitWidth: contribLbl.implicitWidth + 14
                                implicitHeight: contribLbl.implicitHeight + 6

                                QQC2.Label {
                                    id: contribLbl
                                    anchors.centerIn: parent
                                    text: KI18n.i18n("Also active: ") + contribPill.modelData
                                    color: Kirigami.Theme.neutralTextColor
                                    font.bold: true
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: recLabel.implicitHeight + 14
                        radius: theme.innerRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor

                        QQC2.Label {
                            id: recLabel
                            anchors.fill: parent
                            anchors.margins: 7
                            text: KI18n.i18n("Next step: ") + (KI18n.i18n(page.controller.verdictRecommendation) || KI18n.i18n("Review the available measurements below."))
                            wrapMode: Text.WordWrap
                            color: Kirigami.Theme.textColor
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        PulseButton {
                            icon.name: "go-next"
                            text: page.controller.verdictSeverity > 0 && page.controller.verdictSeverity < 4
                                  ? KI18n.i18n("Inspect ") + (page.controller.primaryBottleneckDomain || "details")
                                  : KI18n.i18n("Power & Cooling")
                            isPrimary: page.controller.verdictSeverity > 0 && page.controller.verdictSeverity < 4
                            onClicked: page.navigateRequested(page.targetPageForDomain(page.controller.primaryBottleneckDomain), page.sectionForDomain(page.controller.primaryBottleneckDomain))
                        }

                        PulseButton {
                            icon.name: "chronometer"
                            text: KI18n.i18n("Open History")
                            onClicked: page.navigateRequested("flight", "timeline")
                        }

                        PulseButton {
                            icon.name: "memory"
                            text: KI18n.i18n("Memory & Pressure")
                            onClicked: page.navigateRequested("pressure", "overview")
                        }

                        PulseButton {
                            visible: page.controller.powerProfileServiceAvailable
                                     && page.controller.activePowerProfile === "power-saver"
                                     && page.controller.verdictSeverity >= 1
                                     && page.controller.availablePowerProfiles.indexOf("balanced") >= 0
                            icon.name: "speedometer"
                            text: KI18n.i18n("Switch to Balanced profile")
                            enabled: !page.controller.powerProfileChangePending
                            onClicked: page.controller.setPowerProfile("balanced")
                        }
                    }
                }

                // Right-side Circular Pressure / Responsiveness Ring (visible on medium/wide viewports)
                Rectangle {
                    visible: page.width >= 680
                    Layout.preferredWidth: 136
                    Layout.preferredHeight: 136
                    Layout.alignment: Qt.AlignVCenter
                    radius: theme.innerRadius
                    color: theme.subtleSurfaceColor
                    border.width: 1
                    border.color: theme.subtleBorderColor

                    Canvas {
                        id: respRingCanvas
                        anchors.fill: parent
                        anchors.margins: 10

                        Connections {
                            target: page.controller
                            function onTelemetryUpdated() {
                                respRingCanvas.requestPaint()
                            }
                        }

                        onPaint: {
                            if (width <= 24 || height <= 24) return
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var cx = width / 2
                            var cy = height / 2
                            var r = Math.max(4, Math.min(cx, cy) - 8)
                            var startAngle = 0.75 * Math.PI
                            var endAngle = 2.25 * Math.PI

                            ctx.lineWidth = 8
                            ctx.lineCap = "round"
                            ctx.strokeStyle = theme.trackColor.toString()
                            ctx.beginPath()
                            ctx.arc(cx, cy, r, startAngle, endAngle, false)
                            ctx.stroke()

                            if (page.controller.psiAvailable) {
                                var score = Math.min(100, Math.max(0, page.controller.responsivenessScore)) / 100.0
                                var valAngle = startAngle + score * (endAngle - startAngle)
                                var ringColor = score < 0.80
                                                ? Kirigami.Theme.negativeTextColor
                                                : (score < 0.92 ? Kirigami.Theme.neutralTextColor : theme.psiAccent)
                                ctx.strokeStyle = ringColor.toString()
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, startAngle, valAngle, false)
                                ctx.stroke()
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 0

                        QQC2.Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: page.controller.psiAvailable
                                  ? page.controller.responsivenessScore.toFixed(0) + "/100"
                                  : "N/A"
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 6
                            color: Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: KI18n.i18n("RESPONSIVENESS")
                            font.bold: true
                            font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }

                    QQC2.AbstractButton {
                        objectName: "responsivenessButton"
                        anchors.fill: parent
                        Accessible.name: KI18n.i18n("Responsiveness index ") + (page.controller.psiAvailable ? page.controller.responsivenessScore.toFixed(0) + KI18n.i18n(" out of 100; higher is better") : "unavailable") + KI18n.i18n(". Open pressure details")
                        onClicked: page.navigateRequested("pressure", "overview")
                        background: Rectangle { color: "transparent"; border.width: parent.activeFocus ? 2 : 0; border.color: Kirigami.Theme.focusColor }
                    }
                }
            }
        }

        // 2. Six Interactive Primary Domain Cards with Live Mini-Sparklines
        GridLayout {
            Layout.fillWidth: true
            columns: width < 720 ? 2 : 3
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            Repeater {
                model: [
                    {
                        title: KI18n.i18n("CPU load"),
                        icon: "cpu",
                        accentKey: "cpu",
                        sampleKey: "cpuLoad",
                        sampleAvailKey: "cpuLoadAvailable",
                        sampleMin: 0.0,
                        sampleMax: 100.0,
                        quickActionText: KI18n.i18n("Inspect cores"),
                        value: page.metric(page.controller.cpuLoadAvailable, page.controller.cpuLoadPercent.toFixed(1), "%"),
                        subtitle: page.controller.cpuAvgFreqMhz > 0
                            ? page.controller.cpuAvgFreqMhz + KI18n.i18n(" MHz avg · ") + page.controller.cpuPeakFreqMhz + KI18n.i18n(" MHz peak")
                            : KI18n.i18n("Frequency unavailable"),
                        ratio: page.controller.cpuLoadAvailable ? Math.min(1.0, Math.max(0.0, page.controller.cpuLoadPercent / 100.0)) : 0.0,
                        barAvailable: page.controller.cpuLoadAvailable,
                        targetPage: "power", section: "cpu"
                    },
                    {
                        title: KI18n.i18n("CPU temperature"),
                        icon: "temperature-normal",
                        accentKey: "temp",
                        sampleKey: "cpuTemp",
                        sampleAvailKey: "cpuTempAvailable",
                        sampleMin: 25.0,
                        sampleMax: 100.0,
                        quickActionText: KI18n.i18n("Cooling & fans"),
                        value: page.temp(page.controller.cpuTempAvailable, page.controller.cpuPackageTemp),
                        subtitle: page.controller.thermalHeadroomAvailable
                            ? MeasurementLabels.formatTemperatureDelta(page.controller.thermalHeadroom, page.controller.temperatureUnit) + KI18n.i18n(" headroom")
                            : KI18n.i18n("Thermal limit unavailable"),
                        ratio: page.controller.cpuTempAvailable ? Math.min(1.0, Math.max(0.0, (page.controller.cpuPackageTemp - 25.0) / 75.0)) : 0.0,
                        barAvailable: page.controller.cpuTempAvailable,
                        targetPage: "power", section: "cooling"
                    },
                    {
                        title: KI18n.i18n("Graphics (GPU)"),
                        icon: "video-display",
                        accentKey: "gpu",
                        sampleKey: "gpuBusy",
                        sampleAvailKey: "gpuBusyAvailable",
                        sampleMin: 0.0,
                        sampleMax: 100.0,
                        quickActionText: KI18n.i18n("GPU status"),
                        value: page.controller.gpuBusyAvailable
                            ? page.controller.gpuBusyPercent.toFixed(0) + KI18n.i18n("% load")
                            : (page.controller.gpuFrequencyAvailable ? page.controller.gpuCurrentFreqMhz + KI18n.i18n(" MHz") : KI18n.i18n("Unavailable")),
                        subtitle: (page.controller.gpuTempAvailable ? MeasurementLabels.formatTemperature(page.controller.gpuTempCelsius, page.controller.temperatureUnit) : KI18n.i18n("Temp N/A"))
                            + (page.controller.gpuFrequencyAvailable ? " · " + page.controller.gpuCurrentFreqMhz + KI18n.i18n(" MHz") : "")
                            + (page.controller.gpuVramAvailable ? " · " + Math.round(page.controller.gpuVramUsedMib) + KI18n.i18n(" MiB VRAM") : ""),
                        ratio: page.controller.gpuBusyAvailable
                            ? Math.min(1.0, Math.max(0.0, page.controller.gpuBusyPercent / 100.0))
                            : (page.controller.gpuFrequencyAvailable && page.controller.gpuBoostFreqMhz > 0
                               ? Math.min(1.0, Math.max(0.0, page.controller.gpuCurrentFreqMhz / page.controller.gpuBoostFreqMhz)) : 0.0),
                        barAvailable: page.controller.gpuBusyAvailable || (page.controller.gpuFrequencyAvailable && page.controller.gpuBoostFreqMhz > 0),
                        targetPage: "power", section: "gpu"
                    },
                    {
                        title: KI18n.i18n("Memory pressure"),
                        icon: "memory",
                        accentKey: "psi",
                        sampleKey: "psiMemSome10",
                        sampleAvailKey: "psiMemorySomeAvailable",
                        sampleMin: 0.0,
                        sampleMax: 25.0,
                        quickActionText: KI18n.i18n("View memory"),
                        value: page.metric(page.controller.psiMemoryAvailable, page.controller.psiMemSome10.toFixed(1), KI18n.i18n("% time waiting")),
                        subtitle: page.controller.memoryAvailable
                            ? KI18n.i18n("RAM: ") + page.controller.ramUsedGib.toFixed(1) + " / " + page.controller.ramTotalGib.toFixed(1) + KI18n.i18n(" GiB")
                              + (page.controller.zramCompressionAvailable ? KI18n.i18n(" · ZRAM ") + page.controller.zramCompressionRatio.toFixed(1) + "×" : "")
                            : KI18n.i18n("RAM reading unavailable"),
                        ratio: page.controller.psiMemoryAvailable ? Math.min(1.0, Math.max(0.0, page.controller.psiMemSome10 / 25.0)) : 0.0,
                        barAvailable: page.controller.psiMemoryAvailable,
                        targetPage: "pressure", section: "memory"
                    },
                    {
                        title: KI18n.i18n("Storage & I/O"),
                        icon: "drive-harddisk",
                        accentKey: "storage",
                        sampleKey: "diskReadMibPerSec",
                        sampleAvailKey: "diskThroughputAvailable",
                        sampleMin: 0.0,
                        sampleMax: 250.0,
                        quickActionText: KI18n.i18n("Storage details"),
                        value: page.controller.diskThroughputAvailable
                            ? Number(page.controller.diskReadMibPerSec + page.controller.diskWriteMibPerSec).toFixed(1) + KI18n.i18n(" MiB/s")
                            : page.metric(page.controller.psiIoFullAvailable, page.controller.psiIoFull10.toFixed(1), KI18n.i18n("% I/O wait")),
                        subtitle: (page.controller.diskThroughputAvailable
                            ? KI18n.i18n("R ") + page.controller.diskReadMibPerSec.toFixed(1) + KI18n.i18n(" · W ") + page.controller.diskWriteMibPerSec.toFixed(1) + KI18n.i18n(" MiB/s · ")
                            : "")
                            + (page.controller.netThroughputAvailable
                               ? KI18n.i18n("Net ↓") + page.controller.netRxMibPerSec.toFixed(1) + " ↑" + page.controller.netTxMibPerSec.toFixed(1) + KI18n.i18n(" MiB/s · ")
                                : "")
                            + (page.controller.nvmeTempAvailable ? KI18n.i18n("NVMe ") + MeasurementLabels.formatTemperature(page.controller.nvmeTempCelsius, page.controller.temperatureUnit) : KI18n.i18n("NVMe temp N/A")),
                        ratio: page.controller.diskThroughputAvailable
                            ? Math.min(1.0, Math.max(0.0, (page.controller.diskReadMibPerSec + page.controller.diskWriteMibPerSec) / 500.0))
                            : (page.controller.psiIoFullAvailable ? Math.min(1.0, Math.max(0.0, page.controller.psiIoFull10 / 25.0)) : 0.0),
                        barAvailable: page.controller.diskThroughputAvailable || page.controller.psiIoFullAvailable,
                        targetPage: "pressure", section: "storage"
                    },
                    {
                        title: KI18n.i18n("Package power"),
                        icon: "battery-profile-performance",
                        accentKey: "power",
                        sampleKey: "packageWatts",
                        sampleAvailKey: "packagePowerAvailable",
                        sampleMin: 0.0,
                        sampleMax: Math.max(45.0, page.controller.historyMaxWatts),
                        quickActionText: KI18n.i18n("Power profile"),
                        value: page.controller.packagePowerAvailable
                            ? page.controller.packageWatts.toFixed(1) + KI18n.i18n(" W · ") + (page.controller.packagePowerEstimated ? "Estimated" : "Measured")
                            : KI18n.i18n("Unavailable"),
                        subtitle: KI18n.i18n("Profile: ") + (page.controller.activePowerProfile || KI18n.i18n("Unavailable"))
                            + (page.controller.fanAvailable ? " · " + page.controller.primaryFanRpm + KI18n.i18n(" RPM") : ""),
                        ratio: page.controller.packagePowerAvailable
                            ? Math.min(1.0, Math.max(0.0, page.controller.packageWatts / Math.max(25.0, page.controller.pl1LimitWatts > 0 ? page.controller.pl1LimitWatts : 45.0)))
                            : 0.0,
                        barAvailable: page.controller.packagePowerAvailable,
                        targetPage: "power", section: "profile"
                    }
                ]

                delegate: Rectangle {
                    id: metricCard
                    required property var modelData
                    readonly property color cardAccent: theme.channelColor(metricCard.modelData.accentKey)
                    property bool cardHovered: false
                    Layout.fillWidth: true
                    Layout.minimumWidth: 140
                    implicitHeight: cardInnerCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                    radius: theme.cardRadius
                    color: metricCard.cardHovered ? theme.elevatedSurfaceColor : theme.surfaceColor
                    border.width: metricCard.cardHovered ? 1.5 : 1
                    border.color: metricCard.cardHovered ? metricCard.cardAccent : theme.cardBorderColor

                    // Subtle top accent bar
                    Rectangle {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        height: 2
                        radius: 1
                        color: metricCard.cardAccent
                        opacity: metricCard.cardHovered ? 0.95 : 0.55
                    }

                    // Background mini-sparkline canvas with smooth gradient fill
                    Canvas {
                        id: cardSparkCanvas
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 4
                        height: 48
                        opacity: metricCard.cardHovered ? 0.45 : 0.28

                        Connections {
                            target: page.controller
                            function onTelemetryUpdated() {
                                cardSparkCanvas.requestPaint()
                            }
                        }

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var samples = JSON.parse(page.controller.historyRecentSeriesJson(metricCard.modelData.sampleKey,
                                metricCard.modelData.sampleAvailKey, 48))
                            var count = samples.length
                            if (count < 2) return
                            var sMin = metricCard.modelData.sampleMin
                            var sMax = Math.max(sMin + 1, metricCard.modelData.sampleMax)
                            var col = metricCard.cardAccent

                            var pts = []
                            var firstValid = -1
                            var lastValid = -1
                            for (var i = 0; i < count; ++i) {
                                var s = samples[i]
                                if (!s.available || !isFinite(s.value) || s.gapBefore) { pts.push(null); continue }
                                var px = (i / Math.max(1, count - 1)) * width
                                var norm = Math.min(1, Math.max(0, (s.value - sMin) / (sMax - sMin)))
                                var py = height - norm * (height - 8) - 4
                                pts.push({ x: px, y: py })
                                if (firstValid < 0) firstValid = i
                                lastValid = i
                            }
                            if (firstValid < 0 || lastValid <= firstValid) return

                            // 1. Draw smooth gradient area underneath the curve
                            var grad = ctx.createLinearGradient(0, 0, 0, height)
                            grad.addColorStop(0, Qt.rgba(col.r, col.g, col.b, theme.isDark ? 0.38 : 0.25))
                            grad.addColorStop(1, Qt.rgba(col.r, col.g, col.b, 0.0))
                            ctx.fillStyle = grad
                            ctx.beginPath()
                            ctx.moveTo(pts[firstValid].x, height)
                            for (var k = firstValid; k <= lastValid; ++k) {
                                if (pts[k]) ctx.lineTo(pts[k].x, pts[k].y)
                            }
                            ctx.lineTo(pts[lastValid].x, height)
                            ctx.closePath()
                            ctx.fill()

                            // 2. Draw crisp foreground sparkline
                            ctx.strokeStyle = col.toString()
                            ctx.lineWidth = 2.0
                            ctx.beginPath()
                            var continuous = false
                            for (var j = firstValid; j <= lastValid; ++j) {
                                if (!pts[j]) { continuous = false; continue }
                                if (continuous) ctx.lineTo(pts[j].x, pts[j].y)
                                else ctx.moveTo(pts[j].x, pts[j].y)
                                continuous = true
                            }
                            ctx.stroke()
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: metricCard.cardHovered = true
                        onExited: metricCard.cardHovered = false
                        onClicked: page.navigateRequested(metricCard.modelData.targetPage, metricCard.modelData.section)
                    }

                    ColumnLayout {
                        id: cardInnerCol
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.largeSpacing
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Rectangle {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                radius: 6
                                color: theme.accentTint(metricCard.cardAccent, theme.isDark ? 0.18 : 0.12)
                                Kirigami.Icon {
                                    anchors.centerIn: parent
                                    width: 15
                                    height: 15
                                    source: metricCard.modelData.icon
                                    color: metricCard.cardAccent
                                }
                            }

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
                                Accessible.name: KI18n.i18n("Open ") + metricCard.modelData.title + KI18n.i18n(" details")
                                onClicked: page.navigateRequested(metricCard.modelData.targetPage, metricCard.modelData.section)
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
                            Layout.preferredHeight: 7
                            radius: 3.5
                            color: theme.trackColor

                            Rectangle {
                                visible: metricCard.modelData.barAvailable
                                width: parent.width * metricCard.modelData.ratio
                                height: parent.height
                                radius: 3.5
                                color: metricCard.cardAccent
                            }
                        }

                        QQC2.Label {
                            text: metricCard.modelData.subtitle
                            color: Kirigami.Theme.disabledTextColor
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            QQC2.Button {
                                text: metricCard.modelData.quickActionText || KI18n.i18n("Inspect")
                                icon.name: "go-next"
                                flat: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                onClicked: page.navigateRequested(metricCard.modelData.targetPage, metricCard.modelData.section)
                            }
                        }
                    }
                }
            }
        }

        // 3. PSI Index Info Note
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            text: KI18n.i18n("Responsiveness index: ") + page.controller.responsivenessScore.toFixed(1)
                  + "/100. Higher is better: 100 means little observed pressure; lower values mean more stalled work. This calculated index is not a measured response time."
            visible: page.controller.psiAvailable
        }

        // 4. Concurrent Workload & Reports Action Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: workloadCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1
            border.color: theme.cardBorderColor

            ColumnLayout {
                id: workloadCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        radius: 6
                        color: theme.accentTint(Kirigami.Theme.highlightColor, 0.16)
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: "utilities-system-monitor"
                            color: Kirigami.Theme.highlightColor
                        }
                    }

                    QQC2.Label {
                        text: KI18n.i18n("Top process (concurrent CPU load):")
                        color: Kirigami.Theme.disabledTextColor
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: page.controller.culpritName ? page.controller.culpritName : KI18n.i18n("None / Unavailable")
                        color: Kirigami.Theme.textColor
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        visible: page.controller.culpritName && page.controller.culpritCpuPercent > 0
                        radius: theme.pillRadius
                        color: theme.accentTint(theme.cpuAccent, 0.14)
                        border.width: 1
                        border.color: theme.cpuAccent
                        implicitWidth: cCpuLbl.implicitWidth + 10
                        implicitHeight: cCpuLbl.implicitHeight + 4
                        QQC2.Label {
                            id: cCpuLbl
                            anchors.centerIn: parent
                            text: page.controller.culpritCpuPercent.toFixed(1) + KI18n.i18n("% CPU")
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: theme.cpuAccent
                        }
                    }

                    Rectangle {
                        visible: page.controller.culpritName && page.controller.culpritRssMb > 0
                        radius: theme.pillRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        implicitWidth: cRssLbl.implicitWidth + 10
                        implicitHeight: cRssLbl.implicitHeight + 4
                        QQC2.Label {
                            id: cRssLbl
                            anchors.centerIn: parent
                            text: Math.round(page.controller.culpritRssMb) + KI18n.i18n(" MiB RSS")
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    PulseButton {
                        text: KI18n.i18n("Open System Monitor")
                        icon.name: "utilities-system-monitor"
                        onClicked: page.controller.openSystemMonitor()
                    }
                    PulseButton {
                        text: KI18n.i18n("Copy report")
                        icon.name: "edit-copy"
                        onClicked: page.controller.copyIncidentReportToClipboard()
                    }
                    PulseButton {
                        text: KI18n.i18n("Save report + JSON")
                        icon.name: "document-save"
                        onClicked: page.controller.exportIncidentReportToFile()
                    }
                    PulseButton {
                        visible: page.controller.lastActionPath.length > 0
                        text: KI18n.i18n("Open reports folder")
                        icon.name: "folder-open"
                        onClicked: page.controller.openReportsFolder()
                    }
                    PulseButton {
                        objectName: "measurementDetailsButton"
                        icon.name: page.detailsExpanded ? "collapse-all" : "expand-all"
                        text: page.detailsExpanded ? KI18n.i18n("Hide measurement details") : KI18n.i18n("Show measurement details")
                        checkable: true
                        checked: page.detailsExpanded
                        Accessible.name: text
                        onClicked: page.detailsExpanded = checked
                    }
                }
            }
        }

        // 5. Expandable Sensor and Diagnostic Quality Matrix
        Rectangle {
            visible: page.detailsExpanded
            Layout.fillWidth: true
            implicitHeight: detailsCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1
            border.color: theme.cardBorderColor

            ColumnLayout {
                id: detailsCol
                objectName: "section_measurements"
                focus: true
                Accessible.name: KI18n.i18n("measurements section")
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                Kirigami.Heading {
                    text: KI18n.i18n("Measurement details")
                    level: 3
                    Layout.fillWidth: true
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.mediumSpacing

                    Repeater {
                        model: [
                            KI18n.i18n("GPU load: ") + page.metric(page.controller.gpuBusyAvailable, page.controller.gpuBusyPercent.toFixed(0), "%"),
                            KI18n.i18n("GPU temperature: ") + page.temp(page.controller.gpuTempAvailable, page.controller.gpuTempCelsius),
                            KI18n.i18n("GPU driver: ") + (page.controller.gpuAvailable && page.controller.gpuDriverName ? page.controller.gpuDriverName : KI18n.i18n("Unavailable")),
                            KI18n.i18n("Network I/O: ") + (page.controller.netThroughputAvailable ? ("↓" + page.controller.netRxMibPerSec.toFixed(2) + " / ↑" + page.controller.netTxMibPerSec.toFixed(2) + KI18n.i18n(" MiB/s")) : KI18n.i18n("Unavailable")),
                            KI18n.i18n("Fan: ") + (page.controller.fanAvailable ? page.controller.primaryFanRpm + KI18n.i18n(" RPM") : KI18n.i18n("Unavailable")),
                            KI18n.i18n("Thermal headroom: ") + (page.controller.thermalHeadroomAvailable ? MeasurementLabels.formatTemperatureDelta(page.controller.thermalHeadroom, page.controller.temperatureUnit) : KI18n.i18n("Unavailable")),
                            KI18n.i18n("Active power profile: ") + (page.controller.activePowerProfile || KI18n.i18n("Unavailable"))
                        ]
                        delegate: Rectangle {
                            id: detailChip
                            required property string modelData
                            radius: theme.pillRadius
                            color: theme.subtleSurfaceColor
                            border.width: 1
                            border.color: theme.subtleBorderColor
                            implicitWidth: chipLbl.implicitWidth + 16
                            implicitHeight: chipLbl.implicitHeight + 8

                            QQC2.Label {
                                id: chipLbl
                                anchors.centerIn: parent
                                text: detailChip.modelData
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                        }
                    }
                }

                Kirigami.Heading {
                    text: KI18n.i18n("Data quality and sources")
                    level: 4
                    Layout.fillWidth: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: page.width < 680 ? 1 : 2
                    columnSpacing: Kirigami.Units.mediumSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: Object.keys(page.controller.metricMetadata).map(function(key) { return { key: key, label: MeasurementLabels.metricLabel(key) } })
                        delegate: Rectangle {
                            id: qCard
                            required property var modelData
                            readonly property var quality: page.controller.metricMetadata[qCard.modelData.key]
                            Layout.fillWidth: true
                            implicitHeight: qCol.implicitHeight + 16
                            radius: theme.innerRadius
                            color: theme.subtleSurfaceColor
                            border.width: 1
                            border.color: theme.subtleBorderColor

                            ColumnLayout {
                                id: qCol
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true
                                    QQC2.Label {
                                        text: qCard.modelData.label
                                        font.bold: true
                                        Layout.fillWidth: true
                                    }
                                    QQC2.Label {
                                        text: MeasurementLabels.qualityLabel(qCard.quality ? qCard.quality.state : "unavailable")
                                        font.bold: true
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        color: !qCard.quality || qCard.quality.state === "unavailable"
                                               ? Kirigami.Theme.disabledTextColor
                                               : (qCard.quality.state === "available"
                                                  ? Kirigami.Theme.positiveTextColor
                                                  : Kirigami.Theme.neutralTextColor)
                                    }
                                }

                                QQC2.Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    color: Kirigami.Theme.disabledTextColor
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    text: qCard.quality
                                          ? qCard.quality.source + " · "
                                            + (qCard.quality.sampledAtMs > 0 ? new Date(qCard.quality.sampledAtMs).toLocaleTimeString() : KI18n.i18n("Not sampled"))
                                            + KI18n.i18n(" · Reason: ") + (qCard.quality.detail || KI18n.i18n("No additional detail"))
                                          : KI18n.i18n("Unavailable")
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
