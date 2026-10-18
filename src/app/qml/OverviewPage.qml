pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "MeasurementLabels.js" as MeasurementLabels
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    objectName: "overviewPage"
    title: "Overview"
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
        return available ? value + suffix : "Unavailable"
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
                        activeFocusOnTab: true
                        text: page.controller.verdictHeadline
                        level: 2
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    QQC2.Label {
                        text: page.controller.verdictCausalChain || "No contributing signal is currently available."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.textColor
                    }

                    PulseButton {
                        objectName: "whyStatusButton"
                        text: page.whyExpanded ? "Hide status explanation" : "Why this status?"
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
                                        text: "Rule: " + page.controller.verdictRuleId
                                        font.bold: true
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        color: Kirigami.Theme.textColor
                                    }
                                }

                                QQC2.Label {
                                    text: "Observed for " + page.controller.verdictDurationSeconds.toFixed(1) + " s"
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
                                        text: "Missing: " + parent.modelData
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
                                    text: "Recommendation: " + page.controller.verdictRecommendation
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
                                    text: "Also active: " + contribPill.modelData
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
                            text: "Next step: " + (page.controller.verdictRecommendation || "Review the available measurements below.")
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
                                  ? "Inspect " + (page.controller.primaryBottleneckDomain || "details")
                                  : "Power & Cooling"
                            isPrimary: page.controller.verdictSeverity > 0 && page.controller.verdictSeverity < 4
                            onClicked: page.navigateRequested(page.targetPageForDomain(page.controller.primaryBottleneckDomain), page.sectionForDomain(page.controller.primaryBottleneckDomain))
                        }

                        PulseButton {
                            icon.name: "chronometer"
                            text: "Open History"
                            onClicked: page.navigateRequested("flight", "timeline")
                        }

                        PulseButton {
                            icon.name: "memory"
                            text: "Memory & Pressure"
                            onClicked: page.navigateRequested("pressure", "overview")
                        }

                        PulseButton {
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
                            text: "RESPONSIVENESS"
                            font.bold: true
                            font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }

                    QQC2.AbstractButton {
                        objectName: "responsivenessButton"
                        anchors.fill: parent
                        Accessible.name: "Responsiveness index " + (page.controller.psiAvailable ? page.controller.responsivenessScore.toFixed(0) + " out of 100; higher is better" : "unavailable") + ". Open pressure details"
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
                        title: "CPU load",
                        icon: "cpu",
                        accentKey: "cpu",
                        sampleKey: "cpuLoad",
                        sampleAvailKey: "cpuLoadAvailable",
                        sampleMin: 0.0,
                        sampleMax: 100.0,
                        value: page.metric(page.controller.cpuLoadAvailable, page.controller.cpuLoadPercent.toFixed(1), "%"),
                        subtitle: page.controller.cpuAvgFreqMhz > 0
                            ? page.controller.cpuAvgFreqMhz + " MHz avg · " + page.controller.cpuPeakFreqMhz + " MHz peak"
                            : "Frequency unavailable",
                        ratio: page.controller.cpuLoadAvailable ? Math.min(1.0, Math.max(0.0, page.controller.cpuLoadPercent / 100.0)) : 0.0,
                        barAvailable: page.controller.cpuLoadAvailable,
                        targetPage: "power", section: "cpu"
                    },
                    {
                        title: "CPU temperature",
                        icon: "temperature-normal",
                        accentKey: "temp",
                        sampleKey: "cpuTemp",
                        sampleAvailKey: "cpuTempAvailable",
                        sampleMin: 25.0,
                        sampleMax: 100.0,
                        value: page.metric(page.controller.cpuTempAvailable, page.controller.cpuPackageTemp.toFixed(1), " °C"),
                        subtitle: page.controller.thermalHeadroomAvailable
                            ? page.controller.thermalHeadroom.toFixed(1) + " °C headroom"
                            : "Thermal limit unavailable",
                        ratio: page.controller.cpuTempAvailable ? Math.min(1.0, Math.max(0.0, (page.controller.cpuPackageTemp - 25.0) / 75.0)) : 0.0,
                        barAvailable: page.controller.cpuTempAvailable,
                        targetPage: "power", section: "cpu"
                    },
                    {
                        title: "Graphics (GPU)",
                        icon: "video-display",
                        accentKey: "gpu",
                        sampleKey: "gpuBusy",
                        sampleAvailKey: "gpuBusyAvailable",
                        sampleMin: 0.0,
                        sampleMax: 100.0,
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
                        targetPage: "power", section: "gpu"
                    },
                    {
                        title: "Memory pressure",
                        icon: "memory",
                        accentKey: "psi",
                        sampleKey: "psiMemSome10",
                        sampleAvailKey: "psiMemorySomeAvailable",
                        sampleMin: 0.0,
                        sampleMax: 25.0,
                        value: page.metric(page.controller.psiMemoryAvailable, page.controller.psiMemSome10.toFixed(1), "% time waiting"),
                        subtitle: page.controller.memoryAvailable
                            ? "RAM: " + page.controller.ramUsedGib.toFixed(1) + " / " + page.controller.ramTotalGib.toFixed(1) + " GiB"
                              + (page.controller.zramCompressionAvailable ? " · ZRAM " + page.controller.zramCompressionRatio.toFixed(1) + "×" : "")
                            : "RAM reading unavailable",
                        ratio: page.controller.psiMemoryAvailable ? Math.min(1.0, Math.max(0.0, page.controller.psiMemSome10 / 25.0)) : 0.0,
                        barAvailable: page.controller.psiMemoryAvailable,
                        targetPage: "pressure", section: "memory"
                    },
                    {
                        title: "Storage & I/O",
                        icon: "drive-harddisk",
                        accentKey: "storage",
                        sampleKey: "diskReadMibPerSec",
                        sampleAvailKey: "diskThroughputAvailable",
                        sampleMin: 0.0,
                        sampleMax: 250.0,
                        value: page.controller.diskThroughputAvailable
                            ? Number(page.controller.diskReadMibPerSec + page.controller.diskWriteMibPerSec).toFixed(1) + " MiB/s"
                            : page.metric(page.controller.psiIoFullAvailable, page.controller.psiIoFull10.toFixed(1), "% I/O wait"),
                        subtitle: (page.controller.diskThroughputAvailable
                            ? "R " + page.controller.diskReadMibPerSec.toFixed(1) + " · W " + page.controller.diskWriteMibPerSec.toFixed(1) + " MiB/s · "
                            : "")
                            + (page.controller.netThroughputAvailable
                               ? "Net ↓" + page.controller.netRxMibPerSec.toFixed(1) + " ↑" + page.controller.netTxMibPerSec.toFixed(1) + " MiB/s · "
                               : "")
                            + (page.controller.nvmeTempAvailable ? "NVMe " + page.controller.nvmeTempCelsius.toFixed(0) + " °C" : "NVMe temp N/A"),
                        ratio: page.controller.diskThroughputAvailable
                            ? Math.min(1.0, Math.max(0.0, (page.controller.diskReadMibPerSec + page.controller.diskWriteMibPerSec) / 500.0))
                            : (page.controller.psiIoFullAvailable ? Math.min(1.0, Math.max(0.0, page.controller.psiIoFull10 / 25.0)) : 0.0),
                        barAvailable: page.controller.diskThroughputAvailable || page.controller.psiIoFullAvailable,
                        targetPage: "pressure", section: "storage"
                    },
                    {
                        title: "Package power",
                        icon: "battery-profile-performance",
                        accentKey: "power",
                        sampleKey: "packageWatts",
                        sampleAvailKey: "packagePowerAvailable",
                        sampleMin: 0.0,
                        sampleMax: Math.max(45.0, page.controller.historyMaxWatts),
                        value: page.controller.packagePowerAvailable
                            ? page.controller.packageWatts.toFixed(1) + " W · " + (page.controller.packagePowerEstimated ? "Estimated" : "Measured")
                            : "Unavailable",
                        subtitle: "Profile: " + (page.controller.activePowerProfile || "Unavailable")
                            + (page.controller.fanAvailable ? " · " + page.controller.primaryFanRpm + " RPM" : ""),
                        ratio: page.controller.packagePowerAvailable
                            ? Math.min(1.0, Math.max(0.0, page.controller.packageWatts / Math.max(25.0, page.controller.pl1LimitWatts > 0 ? page.controller.pl1LimitWatts : 45.0)))
                            : 0.0,
                        barAvailable: page.controller.packagePowerAvailable,
                        targetPage: "power", section: "cpu"
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

                    // Background mini-sparkline canvas
                    Canvas {
                        id: cardSparkCanvas
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 6
                        height: 36
                        opacity: 0.32

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
                                metricCard.modelData.sampleAvailKey, 36))
                            var count = samples.length
                            if (count < 2) return
                            var sMin = metricCard.modelData.sampleMin
                            var sMax = Math.max(sMin + 1, metricCard.modelData.sampleMax)
                            var col = metricCard.cardAccent

                            ctx.strokeStyle = col.toString()
                            ctx.lineWidth = 1.8
                            ctx.beginPath()
                            var pts = []
                            for (var i = 0; i < count; ++i) {
                                var s = samples[i]
                                if (!s.available || !isFinite(s.value) || s.gapBefore) { pts.push(null); continue }
                                var px = (i / Math.max(1, count - 1)) * width
                                var norm = Math.min(1, Math.max(0, (s.value - sMin) / (sMax - sMin)))
                                var py = height - norm * (height - 6) - 3
                                pts.push({ x: px, y: py })
                            }
                            if (pts.length < 2) return
                            var continuous = false
                            for (var j = 0; j < pts.length; ++j) {
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
                                Accessible.name: "Open " + metricCard.modelData.title + " details"
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
                    }
                }
            }
        }

        // 3. PSI Index Info Note
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            text: "Responsiveness index: " + page.controller.responsivenessScore.toFixed(1)
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
                        text: "Top process (concurrent CPU load):"
                        color: Kirigami.Theme.disabledTextColor
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: page.controller.culpritName ? page.controller.culpritName : "None / Unavailable"
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
                            text: page.controller.culpritCpuPercent.toFixed(1) + "% CPU"
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
                            text: Math.round(page.controller.culpritRssMb) + " MiB RSS"
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    PulseButton {
                        text: "Open System Monitor"
                        icon.name: "utilities-system-monitor"
                        onClicked: page.controller.openSystemMonitor()
                    }
                    PulseButton {
                        text: "Copy report"
                        icon.name: "edit-copy"
                        onClicked: page.controller.copyIncidentReportToClipboard()
                    }
                    PulseButton {
                        text: "Save report + JSON"
                        icon.name: "document-save"
                        onClicked: page.controller.exportIncidentReportToFile()
                    }
                    PulseButton {
                        visible: page.controller.lastActionPath.length > 0
                        text: "Open reports folder"
                        icon.name: "folder-open"
                        onClicked: page.controller.openReportsFolder()
                    }
                    PulseButton {
                        objectName: "measurementDetailsButton"
                        icon.name: page.detailsExpanded ? "collapse-all" : "expand-all"
                        text: page.detailsExpanded ? "Hide measurement details" : "Show measurement details"
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
                activeFocusOnTab: true
                Accessible.name: "measurements section"
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                Kirigami.Heading {
                    text: "Measurement details"
                    level: 3
                    Layout.fillWidth: true
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.mediumSpacing

                    Repeater {
                        model: [
                            "GPU load: " + page.metric(page.controller.gpuBusyAvailable, page.controller.gpuBusyPercent.toFixed(0), "%"),
                            "GPU temperature: " + page.metric(page.controller.gpuTempAvailable, page.controller.gpuTempCelsius.toFixed(0), " °C"),
                            "GPU driver: " + (page.controller.gpuAvailable && page.controller.gpuDriverName ? page.controller.gpuDriverName : "Unavailable"),
                            "Network I/O: " + (page.controller.netThroughputAvailable ? ("↓" + page.controller.netRxMibPerSec.toFixed(2) + " / ↑" + page.controller.netTxMibPerSec.toFixed(2) + " MiB/s") : "Unavailable"),
                            "Fan: " + (page.controller.fanAvailable ? page.controller.primaryFanRpm + " RPM" : "Unavailable"),
                            "Thermal headroom: " + page.metric(page.controller.thermalHeadroomAvailable, page.controller.thermalHeadroom.toFixed(1), " °C"),
                            "Active power profile: " + (page.controller.activePowerProfile || "Unavailable")
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
                    text: "Data quality and sources"
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
                                            + (qCard.quality.sampledAtMs > 0 ? new Date(qCard.quality.sampledAtMs).toLocaleTimeString() : "Not sampled")
                                            + " · Reason: " + (qCard.quality.detail || "No additional detail")
                                          : "Unavailable"
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
