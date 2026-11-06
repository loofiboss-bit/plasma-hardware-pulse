// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0
import "MeasurementLabels.js" as MeasurementLabels

Kirigami.ScrollablePage {
    id: page
    objectName: "overviewPage"
    title: KI18n.i18nd("loofi-pulse", "Overview")
    titleDelegate: Item {
        width: parent ? parent.width : 0
        height: 0
        visible: false
    }

    required property var controller

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
        textColor: page.Kirigami.Theme.textColor
        highlightColor: page.Kirigami.Theme.highlightColor
    }

    function focusSection(section) {
        if (!section) return true
        if (section === "measurements") page.detailsExpanded = true
        var target = section === "measurements" ? detailsCol : heroBanner
        if (!target) return false
        Qt.callLater(function() {
            var position = target.mapToItem(page.flickable.contentItem, 0, 0)
            page.flickable.contentY = Math.max(0, Math.min(position.y - 12,
                page.flickable.contentHeight - page.flickable.height))
            target.forceActiveFocus(Qt.TabFocusReason)
        })
        return true
    }

    readonly property var domainSeverities: {
        var cpuSev = (page.controller.cpuLoadAvailable && page.controller.cpuLoadPercent > 90) ? 1 : 0
        var tempSev = (page.controller.thermalHeadroomAvailable && page.controller.thermalHeadroom < 10) ? 2
                      : (page.controller.cpuTempAvailable && page.controller.cpuPackageTemp > 85 ? 1 : 0)
        var gpuSev = (page.controller.gpuTempAvailable && page.controller.gpuTempCelsius > 88) ? 2
                     : (page.controller.gpuBusyAvailable && page.controller.gpuBusyPercent > 90 ? 1 : 0)
        var memSev = (page.controller.psiMemoryAvailable && page.controller.psiMemSome10 > 25) ? 2
                     : (page.controller.memoryAvailable && page.controller.ramUsedPercent > 88 ? 1 : 0)
        var ioSev = (page.controller.nvmeWarningActive) ? 3 : ((page.controller.diskWaitProcessCount > 2) ? 1 : 0)
        var pwrSev = (page.controller.batteryPresent && !page.controller.onAcPower && page.controller.batteryPercent < 15) ? 2 : 0

        if (page.controller.verdictSeverity > 0 && page.controller.verdictSeverity < 4) {
            var dom = String(page.controller.primaryBottleneckDomain || "").toLowerCase()
            if (dom.indexOf("cpu") >= 0) cpuSev = Math.max(cpuSev, page.controller.verdictSeverity)
            else if (dom.indexOf("therm") >= 0 || dom.indexOf("temp") >= 0 || dom.indexOf("cool") >= 0) tempSev = Math.max(tempSev, page.controller.verdictSeverity)
            else if (dom.indexOf("gpu") >= 0) gpuSev = Math.max(gpuSev, page.controller.verdictSeverity)
            else if (dom.indexOf("mem") >= 0) memSev = Math.max(memSev, page.controller.verdictSeverity)
            else if (dom.indexOf("stor") >= 0 || dom.indexOf("disk") >= 0 || dom.indexOf("io") >= 0) ioSev = Math.max(ioSev, page.controller.verdictSeverity)
            else if (dom.indexOf("power") >= 0 || dom.indexOf("batt") >= 0) pwrSev = Math.max(pwrSev, page.controller.verdictSeverity)
        }
        return [cpuSev, tempSev, gpuSev, memSev, ioSev, pwrSev]
    }

    ColumnLayout {
        width: page.width
        spacing: page.theme.spacingL

        // 1. Page Header (Title, Subtitle, 5/15/30 min selector, Action Buttons)
        PulsePageHeader {
            Layout.fillWidth: true
            theme: page.theme
            title: KI18n.i18nd("loofi-pulse", "Overview")
            subtitle: KI18n.i18nd("loofi-pulse", "Hardware Health & System Pressure")
            selectedMinutes: Math.round((page.controller.historyRetentionSeconds || 900) / 60)
            showSaveButton: true
            showIncidentButton: true
            onTimeWindowSelected: minutes => {
                page.controller.historyRetentionSeconds = minutes * 60
            }
            onSaveRequested: page.controller.exportIncidentReportToFile()
            onIncidentRequested: page.controller.markIncident()
        }

        // 2. Health Hero Banner (6-domain ring, status badge, headline, evidence chips)
        HealthHero {
            id: heroBanner
            Layout.fillWidth: true
            theme: page.theme
            controller: page.controller
            domainSeverities: page.domainSeverities
        }

        // 3. 2x3 Metric Cards Grid
        GridLayout {
            Layout.fillWidth: true
            columns: page.width < 760 ? 1 : (page.width < 1140 ? 2 : 3)
            rowSpacing: page.theme.spacingM
            columnSpacing: page.theme.spacingM

            // CPU Load Card
            MetricCard {
                Layout.fillWidth: true
                theme: page.theme
                title: KI18n.i18nd("loofi-pulse", "CPU Load")
                channelColor: page.theme.cpuAccent
                value: page.controller.cpuLoadAvailable ? page.controller.cpuLoadPercent.toFixed(1) : "—"
                unit: "%"
                caption: page.controller.cpuAvgFreqMhz > 0
                         ? (page.controller.cpuAvgFreqMhz + " MHz avg · " + page.controller.cpuPeakFreqMhz + " MHz peak")
                         : KI18n.i18nd("loofi-pulse", "8 cores")
                available: page.controller.cpuLoadAvailable
                severity: page.domainSeverities[0]
                trend: JSON.parse(page.controller.historyRecentSeriesJson("cpu.load", "cpu.load.avail", 30))
                trendMin: 0
                trendMax: 100
                onClicked: page.navigateRequested("power", "cpu")
            }

            // CPU Temperature Card
            MetricCard {
                Layout.fillWidth: true
                theme: page.theme
                title: KI18n.i18nd("loofi-pulse", "CPU Temperature")
                channelColor: page.theme.tempAccent
                value: page.controller.cpuTempAvailable ? MeasurementLabels.formatTemperature(page.controller.cpuPackageTemp, page.controller.temperatureUnit) : "—"
                unit: ""
                caption: page.controller.thermalHeadroomAvailable
                         ? (MeasurementLabels.formatTemperatureDelta(page.controller.thermalHeadroom, page.controller.temperatureUnit) + " " + KI18n.i18nd("loofi-pulse", "headroom"))
                         : (page.controller.fanAvailable ? (page.controller.primaryFanRpm + " RPM") : "")
                available: page.controller.cpuTempAvailable
                severity: page.domainSeverities[1]
                trend: JSON.parse(page.controller.historyRecentSeriesJson("cpu.temp", "cpu.temp.avail", 30))
                trendMin: 30
                trendMax: 100
                onClicked: page.navigateRequested("power", "cooling")
            }

            // GPU Card
            MetricCard {
                Layout.fillWidth: true
                theme: page.theme
                title: KI18n.i18nd("loofi-pulse", "Graphics (GPU)")
                channelColor: page.theme.gpuAccent
                value: page.controller.gpuBusyAvailable ? page.controller.gpuBusyPercent.toFixed(0) : "—"
                unit: page.controller.gpuBusyAvailable ? "%" : ""
                caption: page.controller.gpuTempAvailable
                         ? MeasurementLabels.formatTemperature(page.controller.gpuTempCelsius, page.controller.temperatureUnit)
                         : (page.controller.gpuDriverName || KI18n.i18nd("loofi-pulse", "GPU Active"))
                available: page.controller.gpuAvailable
                severity: page.domainSeverities[2]
                trend: JSON.parse(page.controller.historyRecentSeriesJson("gpu.busy", "gpu.busy.avail", 30))
                trendMin: 0
                trendMax: 100
                onClicked: page.navigateRequested("power", "gpu")
            }

            // Memory Pressure Card
            MetricCard {
                Layout.fillWidth: true
                theme: page.theme
                title: KI18n.i18nd("loofi-pulse", "Memory Pressure")
                channelColor: page.theme.psiAccent
                value: page.controller.psiMemoryAvailable ? page.controller.psiMemSome10.toFixed(1) : (page.controller.memoryAvailable ? page.controller.ramUsedPercent.toFixed(0) : "—")
                unit: "%"
                caption: page.controller.memoryAvailable
                         ? ("RAM: " + page.controller.ramUsedGib.toFixed(1) + " / " + page.controller.ramTotalGib.toFixed(1) + " GiB")
                         : KI18n.i18nd("loofi-pulse", "PSI some")
                available: page.controller.memoryAvailable || page.controller.psiMemoryAvailable
                severity: page.domainSeverities[3]
                trend: JSON.parse(page.controller.historyRecentSeriesJson("psi.mem.some10", "psi.mem.some.avail", 30))
                trendMin: 0
                trendMax: 25
                onClicked: page.navigateRequested("pressure", "memory")
            }

            // Storage & I/O Card
            MetricCard {
                Layout.fillWidth: true
                theme: page.theme
                title: KI18n.i18nd("loofi-pulse", "Storage & I/O")
                channelColor: page.theme.storageAccent
                value: Number((page.controller.diskReadMibPerSec || 0) + (page.controller.diskWriteMibPerSec || 0)).toFixed(1)
                unit: "MB/s"
                caption: "R: " + Number(page.controller.diskReadMibPerSec || 0).toFixed(1) + " · W: " + Number(page.controller.diskWriteMibPerSec || 0).toFixed(1) + " MB/s"
                available: page.controller.diskThroughputAvailable
                severity: page.domainSeverities[4]
                trend: JSON.parse(page.controller.historyRecentSeriesJson("disk.throughput", "disk.throughput.avail", 30))
                trendMin: 0
                trendMax: 100
                onClicked: page.navigateRequested("storage", "storage")
            }

            // Package Power Card
            MetricCard {
                Layout.fillWidth: true
                theme: page.theme
                title: KI18n.i18nd("loofi-pulse", "Package Power")
                channelColor: page.theme.powerAccent
                value: page.controller.packagePowerAvailable ? page.controller.packageWatts.toFixed(1) : "—"
                unit: "W"
                caption: page.controller.activePowerProfile
                         ? (KI18n.i18nd("loofi-pulse", "Profile: ") + page.controller.activePowerProfile)
                         : (page.controller.batteryPresent ? (page.controller.batteryPercent + "% BAT") : KI18n.i18nd("loofi-pulse", "AC Power"))
                available: page.controller.packagePowerAvailable
                severity: page.domainSeverities[5]
                trend: JSON.parse(page.controller.historyRecentSeriesJson("package.power", "package.power.avail", 30))
                trendMin: 0
                trendMax: 80
                onClicked: page.navigateRequested("power", "power")
            }
        }

        // 4. Diagnostic Findings Card
        PulseCard {
            Layout.fillWidth: true
            theme: page.theme
            padding: page.theme.spacingM

            ColumnLayout {
                anchors.fill: parent
                spacing: page.theme.spacingS

                RowLayout {
                    Layout.fillWidth: true
                    Kirigami.Heading {
                        text: KI18n.i18nd("loofi-pulse", "Findings")
                        level: 3
                        font.pointSize: page.theme.fontTitle
                        font.weight: Font.DemiBold
                        color: page.theme.textColor
                    }
                    Item { Layout.fillWidth: true }
                    QQC2.Label {
                        text: KI18n.i18nd("loofi-pulse", "sorted by severity")
                        font.pointSize: page.theme.fontCaption
                        color: page.theme.faintTextColor
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    implicitHeight: 1
                    color: page.theme.subtleBorderColor
                }

                // Primary causal verdict finding
                FindingRow {
                    Layout.fillWidth: true
                    theme: page.theme
                    visible: page.controller.verdictSeverity > 0 && page.controller.verdictSeverity < 4
                    severity: page.controller.verdictSeverity
                    ruleId: page.controller.verdictRuleId || "diagnostic.verdict"
                    title: page.controller.verdictHeadline || KI18n.i18nd("loofi-pulse", "Hardware Pressure Observed")
                    evidence: page.controller.verdictAssessment || page.controller.verdictCausalChain || ""
                    actionText: KI18n.i18nd("loofi-pulse", "Investigate →")
                    actionPage: page.controller.primaryBottleneckDomain ? (page.controller.primaryBottleneckDomain.toLowerCase().indexOf("mem") >= 0 ? "pressure" : "power") : "power"
                    actionSection: ""
                    onActionTriggered: (targetPage, targetSection) => page.navigateRequested(targetPage, targetSection)
                }

                // Top process finding (if high CPU load)
                FindingRow {
                    Layout.fillWidth: true
                    theme: page.theme
                    visible: page.controller.culpritName && page.controller.culpritCpuPercent > 10
                    severity: 1
                    ruleId: "process.cpu"
                    title: KI18n.i18nd("loofi-pulse", "Top process: ") + page.controller.culpritName
                    evidence: page.controller.culpritCpuPercent.toFixed(1) + "% CPU · " + Math.round(page.controller.culpritRssMb) + " MiB RSS"
                    actionText: KI18n.i18nd("loofi-pulse", "View processes →")
                    actionPage: "processes"
                    actionSection: ""
                    onActionTriggered: (targetPage, targetSection) => page.navigateRequested(targetPage, targetSection)
                }

                // Nominal state when no warnings
                FindingRow {
                    Layout.fillWidth: true
                    theme: page.theme
                    visible: page.controller.verdictSeverity === 0 || page.controller.verdictSeverity === 4
                    severity: page.controller.verdictSeverity
                    ruleId: "system.health"
                    title: page.controller.verdictSeverity === 4
                           ? KI18n.i18nd("loofi-pulse", "Hardware Telemetry Warming Up")
                           : KI18n.i18nd("loofi-pulse", "All hardware domains nominal")
                    evidence: page.controller.verdictSeverity === 4
                              ? KI18n.i18nd("loofi-pulse", "Collecting initial sensor readings...")
                              : KI18n.i18nd("loofi-pulse", "No current system pressure detected")
                    actionText: ""
                }
            }
        }

        // 5. Expandable Measurement Details & Data Quality
        PulseCard {
            Layout.fillWidth: true
            theme: page.theme
            padding: page.theme.spacingM

            ColumnLayout {
                id: detailsCol
                objectName: "section_measurements"
                anchors.fill: parent
                spacing: page.theme.spacingM

                RowLayout {
                    Layout.fillWidth: true
                    spacing: page.theme.spacingM

                    QQC2.Button {
                        objectName: "measurementDetailsButton"
                        icon.name: page.detailsExpanded ? "collapse-all" : "expand-all"
                        text: page.detailsExpanded ? KI18n.i18nd("loofi-pulse", "Hide measurement details") : KI18n.i18nd("loofi-pulse", "Show measurement details")
                        checkable: true
                        checked: page.detailsExpanded
                        Accessible.name: text
                        onClicked: page.detailsExpanded = checked
                    }

                    Item { Layout.fillWidth: true }

                    QQC2.Button {
                        text: KI18n.i18nd("loofi-pulse", "Open System Monitor")
                        icon.name: "utilities-system-monitor"
                        onClicked: page.controller.openSystemMonitor()
                    }

                    QQC2.Button {
                        text: KI18n.i18nd("loofi-pulse", "Copy Report")
                        icon.name: "edit-copy"
                        onClicked: page.controller.copyIncidentReportToClipboard()
                    }
                }

                // Expanded sensor details & metadata quality matrix
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: page.detailsExpanded
                    spacing: page.theme.spacingM

                    Kirigami.Heading {
                        text: KI18n.i18nd("loofi-pulse", "Data Quality & Telemetry Sources")
                        level: 4
                        font.pointSize: page.theme.fontBody
                        font.weight: Font.DemiBold
                        color: page.theme.textColor
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: page.width < 700 ? 1 : 2
                        columnSpacing: page.theme.spacingM
                        rowSpacing: page.theme.spacingS

                        Repeater {
                            model: Object.keys(page.controller.metricMetadata).map(function(key) {
                                return { key: key, label: MeasurementLabels.metricLabel(key) }
                            })

                            delegate: Rectangle {
                                id: qCard
                                required property var modelData
                                readonly property var quality: page.controller.metricMetadata[qCard.modelData.key]
                                Layout.fillWidth: true
                                implicitHeight: qCol.implicitHeight + 16
                                radius: page.theme.innerRadius
                                color: page.theme.subtleSurfaceColor
                                border.width: 1
                                border.color: page.theme.subtleBorderColor

                                ColumnLayout {
                                    id: qCol
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2

                                    RowLayout {
                                        Layout.fillWidth: true
                                        QQC2.Label {
                                            text: qCard.modelData.label
                                            font.weight: Font.DemiBold
                                            font.pointSize: page.theme.fontCaption
                                            color: page.theme.textColor
                                            Layout.fillWidth: true
                                        }
                                        QQC2.Label {
                                            text: MeasurementLabels.qualityLabel(qCard.quality ? qCard.quality.state : "unavailable")
                                            font.weight: Font.Bold
                                            font.pointSize: page.theme.fontCaption * 0.9
                                            color: !qCard.quality || qCard.quality.state === "unavailable"
                                                   ? page.theme.disabledColor
                                                   : (qCard.quality.state === "available"
                                                      ? page.theme.positiveColor
                                                      : page.theme.neutralColor)
                                        }
                                    }

                                    QQC2.Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        color: page.theme.faintTextColor
                                        font.pointSize: page.theme.fontCaption * 0.9
                                        text: qCard.quality
                                              ? (qCard.quality.source + " · "
                                                 + (qCard.quality.sampledAtMs > 0 ? new Date(qCard.quality.sampledAtMs).toLocaleTimeString() : KI18n.i18nd("loofi-pulse", "Not sampled"))
                                                 + (qCard.quality.detail ? (" · " + qCard.quality.detail) : ""))
                                              : KI18n.i18nd("loofi-pulse", "Unavailable")
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
