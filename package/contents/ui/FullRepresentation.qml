pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

Item {
    id: fullRoot

    required property var controller

    Layout.minimumWidth: Kirigami.Units.gridUnit * 16
    Layout.preferredWidth: Kirigami.Units.gridUnit * 26
    Layout.minimumHeight: Kirigami.Units.gridUnit * 20
    Layout.preferredHeight: Kirigami.Units.gridUnit * 28

    readonly property bool isDark: {
        var c = Kirigami.Theme.backgroundColor
        return (0.299 * c.r + 0.587 * c.g + 0.114 * c.b) < 0.5
    }

    readonly property color cardSurface: isDark
        ? Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(1, 1, 1, 0.055))
        : Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(0, 0, 0, 0.035))
    readonly property color cardHoverSurface: isDark
        ? Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(1, 1, 1, 0.095))
        : Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(0, 0, 0, 0.065))
    readonly property color cardBorder: isDark
        ? Qt.rgba(1, 1, 1, 0.12)
        : Qt.rgba(0, 0, 0, 0.12)
    readonly property color trackBg: isDark
        ? Qt.rgba(1, 1, 1, 0.10)
        : Qt.rgba(0, 0, 0, 0.09)

    readonly property color tempAccent: isDark ? "#fb923c" : "#d97706"
    readonly property color cpuAccent: isDark ? "#38bdf8" : "#0284c7"
    readonly property color psiAccent: isDark ? "#34d399" : "#059669"
    readonly property color powerAccent: isDark ? "#a78bfa" : "#7c3aed"
    readonly property color gpuAccent: isDark ? "#f472b6" : "#db2777"

    property string sparkChannel: "temp"

    function channelColor(key) {
        if (key === "temp") return tempAccent
        if (key === "cpu") return cpuAccent
        if (key === "psi") return psiAccent
        if (key === "power") return powerAccent
        if (key === "battery") return cpuAccent
        if (key === "gpu") return gpuAccent
        return cpuAccent
    }

    function sparkChannelDescription() {
        const mins = Math.round((fullRoot.controller.historyRetentionSeconds || 300) / 60)
        if (sparkChannel === "cpu") return "CPU load · last " + mins + "m · 0–100%"
        if (sparkChannel === "psi") return "Responsiveness index · last " + mins + "m · 0–100 (higher is better)"
        if (sparkChannel === "power") return "CPU package power · last " + mins + "m · W"
        if (sparkChannel === "battery") return "Battery discharge · last " + mins + "m · W"
        return "CPU temperature · last " + mins + "m · 30–100°C"
    }

    function sparkChannelNowText() {
        if (sparkChannel === "cpu") {
            return fullRoot.controller.cpuLoadAvailable
                   ? Math.round(fullRoot.controller.cpuLoadPercent) + "% now" : ""
        }
        if (sparkChannel === "psi") {
            return fullRoot.controller.psiAvailable
                   ? fullRoot.controller.responsivenessScore.toFixed(1) + "/100 now" : ""
        }
        if (sparkChannel === "battery") {
            return fullRoot.controller.batteryPowerAvailable && fullRoot.controller.batteryStatus === "Discharging"
                   ? fullRoot.controller.batteryWatts.toFixed(1) + " W now" : "Unavailable or not discharging"
        }
        if (sparkChannel === "power") {
            return fullRoot.controller.packagePowerAvailable
                   ? fullRoot.controller.packageWatts.toFixed(1) + "W now" : ""
        }
        return fullRoot.controller.cpuTempAvailable
               ? Math.round(fullRoot.controller.cpuPackageTemp) + "°C now" : ""
    }

    function severityColor(sev) {
        if (sev === 4) return Kirigami.Theme.disabledTextColor
        if (sev === 3) return Kirigami.Theme.negativeTextColor
        if (sev === 2) return Kirigami.Theme.negativeTextColor
        if (sev === 1) return Kirigami.Theme.neutralTextColor
        return Kirigami.Theme.positiveTextColor
    }

    function severityBgColor(sev) {
        var c = severityColor(sev)
        return Qt.rgba(c.r, c.g, c.b, isDark ? 0.13 : 0.08)
    }

    function severityBadgeText(sev) {
        if (sev === 4) return "WARMING UP"
        if (sev === 3) return "THROTTLING"
        if (sev === 2) return "BOTTLENECK"
        if (sev === 1) return "ELEVATED"
        return "NOMINAL"
    }

    function qualityText(key, label) {
        const metadata = controller.metricMetadata || {}
        const item = metadata[key]
        if (!item) return label + ": Unavailable — awaiting a sample."
        const state = item.state === "available" ? "Measured"
                    : item.state === "calculated" ? "Calculated"
                    : item.state === "estimated" ? "Estimated"
                    : item.state === "warming-up" ? "Warming up"
                    : "Unavailable"
        const explanation = item.detail || (state === "Unavailable" ? "No valid reading from this source." : "")
        const sampledAt = item.sampledAtMs > 0
                        ? Qt.formatTime(new Date(item.sampledAtMs), "HH:mm:ss")
                        : "Awaiting first sample"
        return label + ": " + state + (explanation ? " — " + explanation : "")
               + " · Source: " + (item.source || "Unknown") + " · Sample: " + sampledAt
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentColumn
            width: parent.width - Kirigami.Units.largeSpacing * 2
            x: Kirigami.Units.largeSpacing
            y: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.mediumSpacing

            // Header Row
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Kirigami.Icon {
                        source: "utilities-system-monitor"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                    }

                    Kirigami.Heading {
                        level: 3
                        text: "Loofi Pulse"
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        visible: fullRoot.controller.activePowerProfile.length > 0
                        radius: 5
                        color: fullRoot.cardSurface
                        border.width: 1
                        border.color: fullRoot.cardBorder
                        implicitWidth: profBadge.implicitWidth + 12
                        implicitHeight: profBadge.implicitHeight + 4

                        PlasmaComponents3.Label {
                            id: profBadge
                            anchors.centerIn: parent
                            text: fullRoot.controller.activePowerProfile
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            font.bold: true
                        }
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    PlasmaComponents3.Button {
                        icon.name: "window-new"
                        text: "Open app"
                        Accessible.name: "Open Loofi Pulse app"
                        onClicked: fullRoot.controller.launchStudioApp("overview")
                    }

                    PlasmaComponents3.Button {
                        icon.name: "chronometer"
                        text: "History"
                        Accessible.name: "Open Loofi Pulse History view"
                        onClicked: fullRoot.controller.launchStudioApp("flight")
                    }

                    PlasmaComponents3.Button {
                        icon.name: "edit-copy"
                        text: "Copy report"
                        Accessible.name: "Copy current incident report"
                        onClicked: fullRoot.controller.copyIncidentReportToClipboard()
                    }

                    PlasmaComponents3.Button {
                        icon.name: "utilities-system-monitor"
                        text: "System Monitor"
                        Accessible.name: "Open KDE System Monitor process view"
                        onClicked: fullRoot.controller.openSystemMonitor()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: fullRoot.controller.lastActionStatus.length > 0 || fullRoot.controller.lastActionPath.length > 0
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.controller.lastActionStatus
                    wrapMode: Text.WordWrap
                    color: fullRoot.controller.lastActionSucceeded
                           ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor
                    Accessible.name: "Action result: " + text
                }
                PlasmaComponents3.Button {
                    visible: fullRoot.controller.lastActionSucceeded && fullRoot.controller.lastActionPath.length > 0
                    text: "Open folder"
                    icon.name: "folder-open"
                    onClicked: fullRoot.controller.openReportsFolder()
                }
            }

            // 1. Causal Diagnosis Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: verdictCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: 10
                color: fullRoot.cardSurface
                border.width: 1.5
                border.color: fullRoot.severityColor(fullRoot.controller.verdictSeverity)

                Rectangle {
                    anchors.fill: parent
                    radius: 10
                    color: fullRoot.severityBgColor(fullRoot.controller.verdictSeverity)
                }

                ColumnLayout {
                    id: verdictCol
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: 5

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle {
                            Layout.preferredWidth: 10
                            Layout.preferredHeight: 10
                            radius: 5
                            color: fullRoot.severityColor(fullRoot.controller.verdictSeverity)
                        }
                        Rectangle {
                            radius: 4
                            color: fullRoot.severityBgColor(fullRoot.controller.verdictSeverity)
                            border.width: 1
                            border.color: fullRoot.severityColor(fullRoot.controller.verdictSeverity)
                            implicitWidth: sevPillLbl.implicitWidth + 10
                            implicitHeight: sevPillLbl.implicitHeight + 3
                            PlasmaComponents3.Label {
                                id: sevPillLbl
                                anchors.centerIn: parent
                                text: fullRoot.severityBadgeText(fullRoot.controller.verdictSeverity)
                                color: fullRoot.severityColor(fullRoot.controller.verdictSeverity)
                                font.bold: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                        }
                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            text: fullRoot.controller.verdictHeadline
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                    }

                    PlasmaComponents3.Button {
                        id: explanationToggle
                        objectName: "widgetStatusExplanationButton"
                        text: "Why this status?"
                        checkable: true
                        Accessible.description: "Expand observed signals, diagnosis duration, missing evidence, and next step."
                    }

                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        visible: explanationToggle.checked
                        text: "Rule: " + fullRoot.controller.verdictRuleId + " · Observed for "
                              + fullRoot.controller.verdictDurationSeconds.toFixed(1) + " s"
                              + "\nSignals: " + (fullRoot.controller.verdictEvidence || []).join("; ")
                              + "\nMissing evidence: " + ((fullRoot.controller.verdictMissingEvidence || []).join("; ") || "No missing signals reported")
                        wrapMode: Text.WordWrap
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    }

                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        visible: explanationToggle.checked
                        text: fullRoot.controller.verdictCausalChain
                        font.family: Kirigami.Theme.defaultFont.family
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.92
                        wrapMode: Text.WordWrap
                    }

                    Flow {
                        Layout.fillWidth: true
                        visible: fullRoot.controller.verdictContributingSignals
                                 && fullRoot.controller.verdictContributingSignals.length > 0
                        spacing: 4

                        Repeater {
                            model: fullRoot.controller.verdictContributingSignals || []
                            delegate: Rectangle {
                                id: contribChip
                                required property string modelData
                                radius: 4
                                color: fullRoot.cardSurface
                                border.width: 1
                                border.color: fullRoot.cardBorder
                                implicitWidth: contribLbl.implicitWidth + 8
                                implicitHeight: contribLbl.implicitHeight + 3

                                PlasmaComponents3.Label {
                                    id: contribLbl
                                    anchors.centerIn: parent
                                    text: "+ " + contribChip.modelData
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    opacity: 0.85
                                }
                            }
                        }
                    }

                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        text: "Next step: " + fullRoot.controller.verdictRecommendation
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        opacity: 0.78
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // 2. Physical Domain Cards Grid (2x2) — Full card clickable with progress bars
            GridLayout {
                Layout.fillWidth: true
                columns: width >= Kirigami.Units.gridUnit * 24 ? 2 : 1
                rowSpacing: Kirigami.Units.smallSpacing
                columnSpacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: [
                        {
                            title: "CPU & SoC Package",
                            targetPage: "power",
                            targetSection: "cpu",
                            accessibleName: "Open CPU and Power details",
                            accentKey: "cpu",
                            headline: (fullRoot.controller.cpuTempAvailable ? Math.round(fullRoot.controller.cpuPackageTemp) + "°C" : "Temp unavailable")
                                      + "  •  " + (fullRoot.controller.cpuLoadAvailable ? Math.round(fullRoot.controller.cpuLoadPercent) + "%" : "Load unavailable"),
                            subtitle: "Thermal margin: "
                                      + (fullRoot.controller.thermalHeadroomAvailable ? Math.round(fullRoot.controller.thermalHeadroom) + "°C" : "Unavailable")
                                      + (fullRoot.controller.cpuAvgFreqMhz > 0 ? " · " + fullRoot.controller.cpuAvgFreqMhz + " MHz" : ""),
                            ratio: fullRoot.controller.cpuLoadAvailable
                                   ? Math.min(1.0, Math.max(0.0, fullRoot.controller.cpuLoadPercent / 100.0)) : 0.0,
                            barVisible: fullRoot.controller.cpuLoadAvailable
                        },
                        {
                            title: "Memory, Disk I/O & Pressure",
                            targetPage: "pressure",
                            targetSection: "overview",
                            accessibleName: "Open Memory and Pressure details",
                            accentKey: "psi",
                            headline: "Responsiveness index: " + (fullRoot.controller.psiAvailable ? fullRoot.controller.responsivenessScore.toFixed(1) + "/100" : "Unavailable")
                                      + " · ZRAM: " + (fullRoot.controller.zramCompressionAvailable
                                      ? fullRoot.controller.zramCompressionRatio.toFixed(1) + "×"
                                      : (fullRoot.controller.zramAvailable ? "Idle" : "Unavailable")),
                            subtitle: "Higher is better. PSI wait · CPU " + (fullRoot.controller.psiCpuAvailable ? fullRoot.controller.psiCpuSome10.toFixed(1) + "%" : "N/A")
                                      + " · Mem " + (fullRoot.controller.psiMemoryAvailable ? fullRoot.controller.psiMemSome10.toFixed(1) + "%" : "N/A")
                                      + (fullRoot.controller.diskThroughputAvailable
                                         ? " · Disk " + Number(fullRoot.controller.diskReadMibPerSec + fullRoot.controller.diskWriteMibPerSec).toFixed(1) + " MiB/s"
                                         : " · I/O " + (fullRoot.controller.psiIoFullAvailable ? fullRoot.controller.psiIoFull10.toFixed(1) + "%" : "N/A"))
                                      + (fullRoot.controller.netThroughputAvailable
                                         ? " · Net " + Number(fullRoot.controller.netRxMibPerSec + fullRoot.controller.netTxMibPerSec).toFixed(1) + " MiB/s"
                                         : ""),
                            ratio: fullRoot.controller.psiAvailable
                                   ? Math.min(1.0, Math.max(0.0, fullRoot.controller.responsivenessScore / 100.0)) : 0.0,
                            barVisible: fullRoot.controller.psiAvailable
                        },
                        {
                            title: "GPU, NVMe & Wireless",
                            targetPage: "power",
                            targetSection: "gpu",
                            accessibleName: "Open GPU and sensor details",
                            accentKey: "gpu",
                            headline: "GPU " + (fullRoot.controller.gpuBusyAvailable ? Math.round(fullRoot.controller.gpuBusyPercent) + "%" : "N/A")
                                      + (fullRoot.controller.gpuTempAvailable ? " (" + Math.round(fullRoot.controller.gpuTempCelsius) + "°C)" : "")
                                      + (fullRoot.controller.gpuFrequencyAvailable ? " · " + fullRoot.controller.gpuCurrentFreqMhz + " MHz" : ""),
                            subtitle: "NVMe " + (fullRoot.controller.nvmeTempAvailable ? Math.round(fullRoot.controller.nvmeTempCelsius) + "°C" : "Unavailable")
                                      + " · Wi-Fi " + (fullRoot.controller.wifiTempAvailable ? Math.round(fullRoot.controller.wifiTempCelsius) + "°C" : "Unavailable"),
                            ratio: fullRoot.controller.gpuBusyAvailable
                                   ? Math.min(1.0, Math.max(0.0, fullRoot.controller.gpuBusyPercent / 100.0)) : 0.0,
                            barVisible: fullRoot.controller.gpuBusyAvailable
                        },
                        {
                            title: "CPU package & battery",
                            targetPage: "power",
                            targetSection: "battery",
                            accessibleName: "Open Power and Cooling details",
                            accentKey: "power",
                            headline: "CPU package: " + (fullRoot.controller.packagePowerAvailable
                                       ? fullRoot.controller.packageWatts.toFixed(1) + " W" + (fullRoot.controller.packagePowerEstimated ? " · Est." : " · Measured")
                                       : "Power unavailable") + "  •  "
                                      + (fullRoot.controller.batteryStateAvailable ? fullRoot.controller.batteryTimeRemainingText : "Battery unavailable"),
                            subtitle: "Battery discharge: " + (fullRoot.controller.batteryPowerAvailable && fullRoot.controller.batteryStatus === "Discharging"
                                      ? fullRoot.controller.batteryWatts.toFixed(1) + " W" : "Unavailable or not discharging")
                                      + " · Power limits: " + (fullRoot.controller.powerLimitsAvailable
                                      ? Math.round(fullRoot.controller.pl1LimitWatts) + "/" + Math.round(fullRoot.controller.pl2LimitWatts) + " W"
                                      : "Unavailable") + " · Fan: "
                                      + (fullRoot.controller.fanAvailable ? (fullRoot.controller.primaryFanRpm + " RPM") : "Fan unavailable"),
                            ratio: fullRoot.controller.packagePowerAvailable
                                   ? Math.min(1.0, Math.max(0.0, fullRoot.controller.packageWatts / Math.max(25.0, fullRoot.controller.pl1LimitWatts > 0 ? fullRoot.controller.pl1LimitWatts : 45.0))) : 0.0,
                            barVisible: fullRoot.controller.packagePowerAvailable
                        }
                    ]

                    delegate: Rectangle {
                        id: domCard
                        required property var modelData
                        readonly property color accentColor: fullRoot.channelColor(domCard.modelData.accentKey)
                        property bool hovered: false
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        implicitHeight: domCardCol.implicitHeight + Kirigami.Units.mediumSpacing * 2
                        radius: 10
                        color: domCard.hovered || domCard.activeFocus ? fullRoot.cardHoverSurface : fullRoot.cardSurface
                        border.width: domCard.hovered || domCard.activeFocus ? 2 : 1
                        border.color: domCard.hovered || domCard.activeFocus ? Kirigami.Theme.focusColor : fullRoot.cardBorder
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: domCard.modelData.accessibleName
                        Accessible.description: domCard.modelData.headline + ". " + domCard.modelData.subtitle
                        Accessible.onPressAction: fullRoot.controller.launchStudioApp(domCard.modelData.targetPage, domCard.modelData.targetSection)
                        Keys.onReturnPressed: fullRoot.controller.launchStudioApp(domCard.modelData.targetPage, domCard.modelData.targetSection)
                        Keys.onEnterPressed: fullRoot.controller.launchStudioApp(domCard.modelData.targetPage, domCard.modelData.targetSection)
                        Keys.onSpacePressed: event => {
                            fullRoot.controller.launchStudioApp(domCard.modelData.targetPage, domCard.modelData.targetSection)
                            event.accepted = true
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onEntered: domCard.hovered = true
                            onExited: domCard.hovered = false
                            onClicked: {
                                domCard.forceActiveFocus()
                                fullRoot.controller.launchStudioApp(domCard.modelData.targetPage, domCard.modelData.targetSection)
                            }
                        }

                        ColumnLayout {
                            id: domCardCol
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.mediumSpacing
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Rectangle {
                                    Layout.preferredWidth: 8
                                    Layout.preferredHeight: 8
                                    radius: 4
                                    color: domCard.accentColor
                                }

                                PlasmaComponents3.Label {
                                    text: domCard.modelData.title
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    font.bold: true
                                    opacity: 0.8
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                PlasmaComponents3.ToolButton {
                                    icon.name: "go-next"
                                    display: PlasmaComponents3.AbstractButton.IconOnly
                                    Accessible.name: domCard.modelData.accessibleName
                                    onClicked: fullRoot.controller.launchStudioApp(domCard.modelData.targetPage, domCard.modelData.targetSection)
                                }
                            }

                            PlasmaComponents3.Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: domCard.modelData.headline
                                font.bold: true
                                font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 1
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 5
                                radius: 2.5
                                color: fullRoot.trackBg

                                Rectangle {
                                    visible: domCard.modelData.barVisible
                                    width: parent.width * domCard.modelData.ratio
                                    height: parent.height
                                    radius: 2.5
                                    color: domCard.accentColor
                                }
                            }

                            PlasmaComponents3.Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: domCard.modelData.subtitle
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                opacity: 0.8
                            }
                        }
                    }
                }
            }

            // 3. Collision-Free Mini Live Sparkline (Dedicated header above Canvas)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: sparkCol.implicitHeight + 16
                radius: 10
                color: fullRoot.cardSurface
                border.width: 1
                border.color: fullRoot.cardBorder

                ColumnLayout {
                    id: sparkCol
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        PlasmaComponents3.Label {
                            text: fullRoot.sparkChannelDescription()
                            font: Kirigami.Theme.smallFont
                            opacity: 0.8
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        PlasmaComponents3.Label {
                            text: fullRoot.sparkChannelNowText()
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: fullRoot.channelColor(fullRoot.sparkChannel)
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 4
                        Repeater {
                            model: [
                                { key: "temp", label: "Temp", objName: "sparkChannelTempButton" },
                                { key: "cpu", label: "CPU", objName: "sparkChannelCpuButton" },
                                { key: "psi", label: "Responsiveness", objName: "sparkChannelPsiButton" },
                                { key: "power", label: "CPU package", objName: "sparkChannelPowerButton" },
                                { key: "battery", label: "Battery", objName: "sparkChannelBatteryButton" }
                            ]
                            delegate: PlasmaComponents3.ToolButton {
                                id: sparkBtn
                                required property var modelData
                                objectName: sparkBtn.modelData.objName
                                text: sparkBtn.modelData.label
                                checkable: true
                                checked: fullRoot.sparkChannel === sparkBtn.modelData.key
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                Accessible.name: "Show " + sparkBtn.modelData.label + " sparkline"
                                onClicked: {
                                    fullRoot.sparkChannel = sparkBtn.modelData.key
                                    miniSparkCanvas.requestPaint()
                                }
                            }
                        }
                    }

                    Canvas {
                        id: miniSparkCanvas
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 3.5
                        Accessible.role: Accessible.Graphic
                        Accessible.name: fullRoot.sparkChannelDescription()
                        Accessible.description: fullRoot.sparkChannelNowText() + ". Missing readings and recording pauses appear as gaps."

                        Connections {
                            target: fullRoot.controller
                            function onTelemetryUpdated() {
                                miniSparkCanvas.requestPaint()
                            }
                        }

                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            const samples = fullRoot.controller.historySamples
                            if (!samples || samples.length < 2) return

                            const firstTime = samples[0].monotonicMs
                            const duration = Math.max(1, samples[samples.length - 1].monotonicMs - firstTime)
                            const ch = fullRoot.sparkChannel
                            const accent = fullRoot.channelColor(ch)

                            const powerKey = ch === "battery" ? "batteryWatts" : "packageWatts"
                            const powerAvailableKey = ch === "battery" ? "batteryDischargeAvailable" : "packagePowerAvailable"
                            let maxPower = 45.0
                            if (ch === "power" || ch === "battery") {
                                for (let m = 0; m < samples.length; ++m) {
                                    if (samples[m][powerAvailableKey] && samples[m][powerKey] > maxPower)
                                        maxPower = samples[m][powerKey] * 1.15
                                }
                            }

                            const segments = []
                            let points = []
                            for (let i = 0; i < samples.length; ++i) {
                                const sample = samples[i]
                                const available = ch === "cpu" ? sample.cpuLoadAvailable
                                                : ch === "psi" ? sample.psiAvailable
                                                : ch === "power" || ch === "battery" ? sample[powerAvailableKey]
                                                : sample.cpuTempAvailable
                                if ((!available || sample.gapBefore) && points.length > 0) {
                                    segments.push(points)
                                    points = []
                                }
                                if (!available) continue
                                let norm = 0.0
                                if (ch === "cpu") norm = sample.cpuLoad / 100.0
                                else if (ch === "psi") norm = sample.responsivenessScore / 100.0
                                else if (ch === "power" || ch === "battery") norm = sample[powerKey] / maxPower
                                else norm = (sample.cpuTemp - 30) / 70
                                norm = Math.min(1, Math.max(0, norm))
                                const x = (sample.monotonicMs - firstTime) / duration * width
                                const y = height - (norm * (height - 8)) - 4
                                points.push({ x: x, y: y })
                            }
                            if (points.length) segments.push(points)
                            ctx.lineWidth = 2
                            ctx.strokeStyle = accent.toString()
                            for (let segment of segments) {
                                if (segment.length < 2) continue
                                ctx.beginPath()
                                ctx.moveTo(segment[0].x, segment[0].y)
                                for (let point = 1; point < segment.length; ++point)
                                    ctx.lineTo(segment[point].x, segment[point].y)
                                ctx.stroke()
                            }
                        }
                    }
                }
            }

            // 4. Power Profile Quick Switcher
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: "Power profile: " + (fullRoot.controller.powerProfileChangePending ? "Changing…"
                          : (fullRoot.controller.activePowerProfile || "Unavailable"))
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    font.bold: true
                    opacity: 0.85
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    Repeater {
                        model: fullRoot.controller.availablePowerProfiles
                        delegate: PlasmaComponents3.Button {
                            required property string modelData
                            text: modelData
                            checked: fullRoot.controller.activePowerProfile === modelData
                            checkable: true
                            enabled: fullRoot.controller.powerProfileServiceAvailable && !fullRoot.controller.powerProfileChangePending
                            Accessible.name: "Select " + modelData + " power profile"
                            onClicked: fullRoot.controller.setPowerProfile(modelData)
                        }
                    }
                }
                PlasmaComponents3.Label {
                    visible: !fullRoot.controller.powerProfileServiceAvailable
                    text: "Power profile service unavailable"
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    opacity: 0.7
                }
            }

            PlasmaComponents3.Button {
                id: qualityToggle
                Layout.fillWidth: true
                icon.name: qualityToggle.checked ? "collapse-all" : "expand-all"
                text: "Measurement details"
                checkable: true
                Accessible.name: "Show measurement details, sources, and sample times"
                Accessible.description: "Expand details for CPU load, CPU temperature, responsiveness index, and CPU package power"
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: qualityToggle.checked
                spacing: Kirigami.Units.smallSpacing
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.qualityText("cpuLoad", "CPU load")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.qualityText("cpuTemperature", "CPU temperature")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.qualityText("pressureIndex", "Responsiveness index")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.qualityText("packagePower", "CPU package power")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.qualityText("batteryDischarge", "Battery discharge")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: fullRoot.qualityText("netThroughput", "Network throughput")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
            }
        }
    }
}
