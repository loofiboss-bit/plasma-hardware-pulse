pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: psiPage
    objectName: "pressurePage"
    title: "Memory & Pressure"
    titleDelegate: Item {
        width: parent ? parent.width : 0
        height: 0
        visible: false
    }

    required property var controller

    function focusSection(section) {
        if (!section) return true
        var targets = {"overview": expRow, "cpu": cpuPressureColumn, "memory": memoryPressureColumn, "storage": ioPressureColumn, "network": networkReadout, "allocation": zramCol, "processes": processesHeading}
        var target = targets[section]
        if (!target) return false

        Qt.callLater(function() {
            var position = target.mapToItem(psiPage.flickable.contentItem, 0, 0)
            psiPage.flickable.contentY = Math.max(0, Math.min(position.y - 12,
                psiPage.flickable.contentHeight - psiPage.flickable.height))
            target.forceActiveFocus(Qt.TabFocusReason)
        })
        return true
    }

    property string processSortMode: "combined"
    property string expandedProcessIdentity: ""

    function sortedProcesses() {
        var list = psiPage.controller.topProcesses ? psiPage.controller.topProcesses.slice() : []
        var mode = psiPage.processSortMode
        list.sort(function(a, b) {
            if (mode === "cpu") {
                return Number(b.cpuPercent || 0) - Number(a.cpuPercent || 0)
            }
            if (mode === "rss") {
                return Number(b.rssMegabytes || 0) - Number(a.rssMegabytes || 0)
            }
            if (mode === "disk") {
                var aDisk = a.inDiskWait ? 1 : 0
                var bDisk = b.inDiskWait ? 1 : 0
                if (aDisk !== bDisk) return bDisk - aDisk
                return Number(b.cpuPercent || 0) - Number(a.cpuPercent || 0)
            }
            var aScore = (a.inDiskWait ? 1000 : 0) + Number(a.cpuPercent || 0) + Number(a.rssMegabytes || 0) * 0.015
            var bScore = (b.inDiskWait ? 1000 : 0) + Number(b.cpuPercent || 0) + Number(b.rssMegabytes || 0) * 0.015
            return bScore - aScore
        })
        return list
    }

    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: psiPage.Kirigami.Theme.backgroundColor
        disabledColor: psiPage.Kirigami.Theme.disabledTextColor
        negativeColor: psiPage.Kirigami.Theme.negativeTextColor
        neutralColor: psiPage.Kirigami.Theme.neutralTextColor
        positiveColor: psiPage.Kirigami.Theme.positiveTextColor
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        // 1. Hero PSI Explanation & Circular Responsiveness Score Banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: expRow.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1.5
            border.color: !psiPage.controller.psiAvailable
                          ? theme.cardBorderColor
                          : (psiPage.controller.responsivenessScore < 85
                             ? Kirigami.Theme.negativeTextColor
                             : (psiPage.controller.responsivenessScore < 94 ? Kirigami.Theme.neutralTextColor : theme.psiAccent))

            RowLayout {
                id: expRow
                objectName: "section_overview"
                activeFocusOnTab: true
                Accessible.name: "overview section"
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26
                            radius: 6
                            color: theme.accentTint(theme.psiAccent, theme.isDark ? 0.18 : 0.12)
                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                source: "memory"
                                color: theme.psiAccent
                            }
                        }

                        Kirigami.Heading {
                            level: 2
                            text: "Memory & system pressure (PSI)"
                            color: Kirigami.Theme.textColor
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "Linux Pressure Stall Information (PSI) measures time tasks spend waiting for CPU, memory, or storage. High RAM use alone does not mean the system is under pressure. The Responsiveness index runs from 0 to 100; higher is better, with 100 meaning little observed pressure. Raw PSI wait percentages increase with stalled work."
                        color: Kirigami.Theme.textColor
                        wrapMode: Text.WordWrap
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            radius: theme.pillRadius
                            color: theme.accentTint(theme.psiAccent, theme.isDark ? 0.16 : 0.10)
                            border.width: 1
                            border.color: theme.psiAccent
                            implicitWidth: scorePillLbl.implicitWidth + 16
                            implicitHeight: scorePillLbl.implicitHeight + 8

                            QQC2.Label {
                                id: scorePillLbl
                                anchors.centerIn: parent
                                text: "Responsiveness index: " + (psiPage.controller.psiAvailable
                                      ? psiPage.controller.responsivenessScore.toFixed(1) + "/100" : "Unavailable")
                                font.bold: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                color: Kirigami.Theme.textColor
                            }
                        }

                        Rectangle {
                            radius: theme.pillRadius
                            color: theme.subtleSurfaceColor
                            border.width: 1
                            border.color: theme.subtleBorderColor
                            implicitWidth: stallPillLbl.implicitWidth + 16
                            implicitHeight: stallPillLbl.implicitHeight + 8

                            QQC2.Label {
                                id: stallPillLbl
                                anchors.centerIn: parent
                                text: "Total stall rate: " + Math.round(psiPage.controller.psiCpuStallMs + psiPage.controller.psiMemStallMs + psiPage.controller.psiIoStallMs) + " ms/s"
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }
                }

                // Circular Pressure Gauge
                Rectangle {
                    visible: psiPage.width >= 660
                    Layout.preferredWidth: 124
                    Layout.preferredHeight: 124
                    Layout.alignment: Qt.AlignVCenter
                    radius: theme.innerRadius
                    color: theme.subtleSurfaceColor
                    border.width: 1
                    border.color: theme.subtleBorderColor

                    Canvas {
                        id: psiGaugeCanvas
                        anchors.fill: parent
                        anchors.margins: 8

                        Connections {
                            target: psiPage.controller
                            function onTelemetryUpdated() {
                                psiGaugeCanvas.requestPaint()
                            }
                        }

                        onPaint: {
                            if (width <= 24 || height <= 24) return
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var cx = width / 2
                            var cy = height / 2
                            var r = Math.max(4, Math.min(cx, cy) - 7)
                            var startAngle = 0.75 * Math.PI
                            var endAngle = 2.25 * Math.PI

                            ctx.lineWidth = 8
                            ctx.lineCap = "round"
                            ctx.strokeStyle = theme.trackColor.toString()
                            ctx.beginPath()
                            ctx.arc(cx, cy, r, startAngle, endAngle, false)
                            ctx.stroke()

                            if (psiPage.controller.psiAvailable) {
                                var score = Math.min(100, Math.max(0, psiPage.controller.responsivenessScore)) / 100.0
                                var valAngle = startAngle + score * (endAngle - startAngle)
                                var col = score < 0.85 ? Kirigami.Theme.negativeTextColor
                                                       : (score < 0.94 ? Kirigami.Theme.neutralTextColor : theme.psiAccent)
                                ctx.strokeStyle = col.toString()
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
                            text: psiPage.controller.psiAvailable
                                  ? psiPage.controller.responsivenessScore.toFixed(1) + "/100"
                                  : "N/A"
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.defaultFont.pixelSize + 4
                        }
                        QQC2.Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: "RESPONSIVENESS"
                            font.bold: true
                            font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                }
            }
        }

        // 2. 3-Axis PSI Radar Cards (CPU, Memory, Storage & I/O)
        GridLayout {
            Layout.fillWidth: true
            columns: width < 680 ? 1 : 3
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            // CPU PSI Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: cpuPressureColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: theme.cardRadius
                color: theme.surfaceColor
                border.width: psiPage.controller.psiCpuSome10 > 10 ? 1.5 : 1
                border.color: !psiPage.controller.psiCpuAvailable
                              ? theme.cardBorderColor
                              : (psiPage.controller.psiCpuSome10 > 10 ? Kirigami.Theme.neutralTextColor : theme.cardBorderColor)

                ColumnLayout {
                    id: cpuPressureColumn
                objectName: "section_cpu"
                activeFocusOnTab: true
                Accessible.name: "cpu section"
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        QQC2.Label {
                            text: "CPU PSI wait"
                            color: theme.cpuAccent
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            radius: theme.pillRadius
                            color: theme.subtleSurfaceColor
                            implicitWidth: cpuStallBadge.implicitWidth + 10
                            implicitHeight: cpuStallBadge.implicitHeight + 4
                            QQC2.Label {
                                id: cpuStallBadge
                                anchors.centerIn: parent
                                text: psiPage.controller.psiCpuAvailable ? Math.round(psiPage.controller.psiCpuStallMs) + " ms/s" : "N/A"
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                font.bold: true
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    QQC2.Label {
                        text: psiPage.controller.psiCpuAvailable
                              ? ("PSI wait some: " + psiPage.controller.psiCpuSome10.toFixed(2) + "%  ("
                                 + Math.round(psiPage.controller.psiCpuStallMs) + " ms/s)")
                              : "Unavailable"
                        color: Kirigami.Theme.textColor
                        font.bold: true
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        radius: 4
                        color: theme.trackColor
                        Rectangle {
                            visible: psiPage.controller.psiCpuAvailable
                            width: parent.width * Math.min(1.0, Math.max(0.0, psiPage.controller.psiCpuSome10 / 25.0))
                            height: parent.height
                            radius: 4
                            color: psiPage.controller.psiCpuSome10 > 10 ? Kirigami.Theme.neutralTextColor : theme.cpuAccent
                        }
                    }

                    QQC2.Label {
                        text: "Percentage of time at least one runnable thread is delayed waiting for CPU time."
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // Memory PSI Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: memoryPressureColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: theme.cardRadius
                color: theme.surfaceColor
                border.width: psiPage.controller.psiMemSome10 > 2 ? 1.5 : 1
                border.color: psiPage.controller.psiMemSome10 > 2 ? Kirigami.Theme.negativeTextColor : theme.cardBorderColor

                ColumnLayout {
                    id: memoryPressureColumn
                objectName: "section_memory"
                activeFocusOnTab: true
                Accessible.name: "memory section"
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        QQC2.Label {
                            text: "Memory PSI wait"
                            color: theme.psiAccent
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            radius: theme.pillRadius
                            color: theme.subtleSurfaceColor
                            implicitWidth: memStallBadge.implicitWidth + 10
                            implicitHeight: memStallBadge.implicitHeight + 4
                            QQC2.Label {
                                id: memStallBadge
                                anchors.centerIn: parent
                                text: psiPage.controller.psiMemoryAvailable ? Math.round(psiPage.controller.psiMemStallMs) + " ms/s" : "N/A"
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                font.bold: true
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    QQC2.Label {
                        text: psiPage.controller.psiMemoryAvailable
                              ? ("PSI wait some: " + psiPage.controller.psiMemSome10.toFixed(2) + "% | full wait: "
                                 + (psiPage.controller.psiMemoryFullAvailable ? psiPage.controller.psiMemFull10.toFixed(2) + "%" : "Unavailable")
                                 + " (" + Math.round(psiPage.controller.psiMemStallMs) + " ms/s)")
                              : "Unavailable"
                        color: Kirigami.Theme.textColor
                        font.bold: true
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    // Some & Full Dual Bars
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            radius: 3
                            color: theme.trackColor
                            Rectangle {
                                visible: psiPage.controller.psiMemoryAvailable
                                width: parent.width * Math.min(1.0, Math.max(0.0, psiPage.controller.psiMemSome10 / 20.0))
                                height: parent.height
                                radius: 3
                                color: psiPage.controller.psiMemSome10 >= 5 ? Kirigami.Theme.negativeTextColor : theme.psiAccent
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 5
                            radius: 2.5
                            color: theme.trackColor
                            Rectangle {
                                visible: psiPage.controller.psiMemoryFullAvailable
                                width: parent.width * Math.min(1.0, Math.max(0.0, psiPage.controller.psiMemFull10 / 20.0))
                                height: parent.height
                                radius: 2.5
                                color: Kirigami.Theme.negativeTextColor
                            }
                        }
                    }

                    QQC2.Label {
                        text: "Measures actual thread stall time spent on ZRAM compression, page reclaim, and allocation."
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // Storage & I/O PSI Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: ioPressureColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: theme.cardRadius
                color: theme.surfaceColor
                border.width: psiPage.controller.psiIoFull10 > 5 ? 1.5 : 1
                border.color: psiPage.controller.psiIoFull10 > 5 ? Kirigami.Theme.neutralTextColor : theme.cardBorderColor

                ColumnLayout {
                    id: ioPressureColumn
                objectName: "section_storage"
                activeFocusOnTab: true
                Accessible.name: "storage section"
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        QQC2.Label {
                            text: "Storage and I/O PSI wait"
                            color: theme.storageAccent
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            radius: theme.pillRadius
                            color: theme.subtleSurfaceColor
                            implicitWidth: ioStallBadge.implicitWidth + 10
                            implicitHeight: ioStallBadge.implicitHeight + 4
                            QQC2.Label {
                                id: ioStallBadge
                                anchors.centerIn: parent
                                text: psiPage.controller.psiIoAvailable ? Math.round(psiPage.controller.psiIoStallMs) + " ms/s" : "N/A"
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                font.bold: true
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    QQC2.Label {
                        text: psiPage.controller.psiIoAvailable
                              ? ("PSI wait some: " + psiPage.controller.psiIoSome10.toFixed(2) + "% | full wait: "
                                 + (psiPage.controller.psiIoFullAvailable ? psiPage.controller.psiIoFull10.toFixed(2) + "%" : "Unavailable")
                                 + " (" + Math.round(psiPage.controller.psiIoStallMs) + " ms/s)")
                              : "Unavailable"
                        color: Kirigami.Theme.textColor
                        font.bold: true
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        radius: 4
                        color: theme.trackColor
                        Rectangle {
                            visible: psiPage.controller.psiIoAvailable
                            width: parent.width * Math.min(1.0, Math.max(0.0, psiPage.controller.psiIoSome10 / 25.0))
                            height: parent.height
                            radius: 4
                            color: psiPage.controller.psiIoFull10 >= 8 ? Kirigami.Theme.negativeTextColor : theme.storageAccent
                        }
                    }

                    QQC2.Label {
                        visible: psiPage.controller.diskThroughputAvailable
                        text: "Disk throughput: " + psiPage.controller.diskReadMibPerSec.toFixed(1) + " MiB/s read · "
                              + psiPage.controller.diskWriteMibPerSec.toFixed(1) + " MiB/s write"
                        color: theme.storageAccent
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        font.bold: true
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    QQC2.Label {
                        id: networkReadout
                objectName: "section_network"
                        activeFocusOnTab: true
                        text: psiPage.controller.netThroughputAvailable
                              ? "Network throughput: ↓" + psiPage.controller.netRxMibPerSec.toFixed(2) + " MiB/s · ↑"
                                + psiPage.controller.netTxMibPerSec.toFixed(2) + " MiB/s"
                              : "Network throughput: Unavailable"
                        color: theme.psiAccent
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        font.bold: true
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    QQC2.Label {
                        text: psiPage.controller.diskWaitProcessCount > 0
                              ? (psiPage.controller.diskWaitProcessCount + " process(es) currently in uninterruptible disk wait (state D).")
                              : "Indicates when threads are blocked waiting for synchronous NVMe/disk transactions."
                        color: psiPage.controller.diskWaitProcessCount > 0 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor
                        font.bold: psiPage.controller.diskWaitProcessCount > 0
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // 3. Visual Memory, ZRAM Compression & Swap Pipeline
        Kirigami.Heading {
            level: 3
            text: "Memory use, compression and swap"
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: zramCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1
            border.color: theme.cardBorderColor

            ColumnLayout {
                id: zramCol
                objectName: "section_allocation"
                activeFocusOnTab: true
                Accessible.name: "allocation section"
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                GridLayout {
                    Layout.fillWidth: true
                    columns: psiPage.width < 660 ? 1 : 3
                    columnSpacing: Kirigami.Units.smallSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: ramTileCol.implicitHeight + 16
                        radius: theme.innerRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        ColumnLayout {
                            id: ramTileCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            QQC2.Label {
                                text: "Physical RAM"
                                color: theme.cpuAccent
                                font.bold: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                            QQC2.Label {
                                text: psiPage.controller.memoryAvailable
                                      ? (psiPage.controller.ramUsedGib.toFixed(2) + " GiB / "
                                         + psiPage.controller.ramTotalGib.toFixed(2) + " GiB ("
                                         + psiPage.controller.ramUsedPercent.toFixed(1) + "%)")
                                      : "Unavailable"
                                font.bold: true
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: zramTileCol.implicitHeight + 16
                        radius: theme.innerRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        ColumnLayout {
                            id: zramTileCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            QQC2.Label {
                                text: "ZRAM Compression"
                                color: theme.psiAccent
                                font.bold: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                            QQC2.Label {
                                text: !psiPage.controller.zramAvailable ? "ZRAM: Unavailable"
                                      : (psiPage.controller.zramCompressionAvailable
                                         ? ("ZRAM: " + psiPage.controller.zramCompressionRatio.toFixed(2) + "× compression")
                                         : "ZRAM: No compressed pages in use")
                                font.bold: true
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: swapTileCol.implicitHeight + 16
                        radius: theme.innerRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        ColumnLayout {
                            id: swapTileCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            QQC2.Label {
                                text: "Swap Allocation"
                                color: theme.powerAccent
                                font.bold: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            }
                            QQC2.Label {
                                text: psiPage.controller.swapAvailable
                                      ? "Swap used: " + psiPage.controller.swapUsedGib.toFixed(2) + " / "
                                        + psiPage.controller.swapTotalGib.toFixed(2) + " GiB"
                                      : "Swap: Unavailable"
                                font.bold: true
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                // Segmented RAM & ZRAM Pipeline Bar
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 16
                        radius: 8
                        color: theme.trackColor
                        clip: true

                        Rectangle {
                            visible: psiPage.controller.memoryAvailable
                            width: parent.width * Math.min(1.0, Math.max(0, psiPage.controller.ramUsedPercent / 100.0))
                            height: parent.height
                            radius: 8
                            color: theme.cpuAccent
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: !psiPage.controller.zramAvailable ? "ZRAM details: Unavailable"
                              : ("ZRAM data: " + Math.round(psiPage.controller.zramOrigMib) + " MiB before compression, "
                                 + Math.round(psiPage.controller.zramComprMib) + " MiB compressed payload and "
                                 + Math.round(psiPage.controller.zramMemUsedMib) + " MiB allocator memory.")
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        // 4. Concurrent Resource Use (Process Radar Table)
        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Heading {
                level: 3
                id: processesHeading
                objectName: "section_processes"
                activeFocusOnTab: true
                text: "Concurrent resource use"
            }

            Repeater {
                model: [
                    { key: "combined", label: "Sort: Combined" },
                    { key: "cpu", label: "Sort: CPU %" },
                    { key: "rss", label: "Sort: Memory RSS" },
                    { key: "disk", label: "Sort: Disk Wait" }
                ]
                delegate: PulseButton {
                    id: sortBtn
                    required property var modelData
                    text: sortBtn.modelData.label
                    checkable: true
                    checked: psiPage.processSortMode === sortBtn.modelData.key
                    onClicked: psiPage.processSortMode = sortBtn.modelData.key
                }
            }

            PulseButton {
                icon.name: "utilities-system-monitor"
                text: "Open System Monitor"
                onClicked: psiPage.controller.openSystemMonitor()
            }
        }

        Repeater {
            model: psiPage.sortedProcesses()
            delegate: Rectangle {
                id: processCard
                required property var modelData
                readonly property string identity: String(modelData.pid) + ":" + String(modelData.startTimeTicks)
                readonly property bool expanded: psiPage.expandedProcessIdentity === identity
                Layout.fillWidth: true
                implicitHeight: pCardCol.implicitHeight + Kirigami.Units.smallSpacing * 2
                radius: theme.innerRadius
                color: processCard.expanded ? theme.elevatedSurfaceColor : theme.surfaceColor
                border.width: 1
                border.color: processCard.expanded ? Kirigami.Theme.highlightColor : theme.cardBorderColor

                ColumnLayout {
                    id: pCardCol
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.smallSpacing
                    spacing: 4

                    QQC2.ToolButton {
                        Layout.fillWidth: true
                        objectName: "processRow"
                        icon.name: processCard.expanded ? "arrow-down" : "arrow-right"
                        text: processCard.modelData.name + " · " + Number(processCard.modelData.cpuPercent).toFixed(1)
                              + "% CPU · " + Math.round(Number(processCard.modelData.rssMegabytes)) + " MiB"
                              + (processCard.modelData.inDiskWait ? " · DISK WAIT (D)" : "")
                        checkable: true
                        checked: processCard.expanded
                        Accessible.name: text + (checked ? ". Collapse process details" : ". Expand process details")
                        onClicked: psiPage.expandedProcessIdentity = processCard.expanded ? "" : processCard.identity

                        contentItem: RowLayout {
                            spacing: 8
                            Kirigami.Icon {
                                source: processCard.expanded ? "arrow-down" : "arrow-right"
                                Layout.preferredWidth: 12
                                Layout.preferredHeight: 12
                                color: Kirigami.Theme.disabledTextColor
                            }
                            QQC2.Label {
                                text: processCard.modelData.name
                                font.bold: true
                                color: Kirigami.Theme.textColor
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                visible: processCard.modelData.inDiskWait
                                radius: theme.pillRadius
                                color: theme.accentTint(Kirigami.Theme.negativeTextColor, 0.20)
                                border.width: 1
                                border.color: Kirigami.Theme.negativeTextColor
                                implicitWidth: dwLbl.implicitWidth + 8
                                implicitHeight: dwLbl.implicitHeight + 4
                                QQC2.Label { id: dwLbl; anchors.centerIn: parent; text: "DISK WAIT (D)"; font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2); font.bold: true; color: Kirigami.Theme.negativeTextColor }
                            }
                            Rectangle {
                                radius: theme.pillRadius
                                color: theme.accentTint(theme.cpuAccent, 0.14)
                                border.width: 1
                                border.color: theme.cpuAccent
                                implicitWidth: cpuPillLbl.implicitWidth + 8
                                implicitHeight: cpuPillLbl.implicitHeight + 4
                                QQC2.Label { id: cpuPillLbl; anchors.centerIn: parent; text: Number(processCard.modelData.cpuPercent).toFixed(1) + "% CPU"; font.pixelSize: Kirigami.Theme.smallFont.pixelSize; font.bold: true; color: theme.cpuAccent }
                            }
                            Rectangle {
                                radius: theme.pillRadius
                                color: theme.subtleSurfaceColor
                                border.width: 1
                                border.color: theme.subtleBorderColor
                                implicitWidth: rssPillLbl.implicitWidth + 8
                                implicitHeight: rssPillLbl.implicitHeight + 4
                                QQC2.Label { id: rssPillLbl; anchors.centerIn: parent; text: Math.round(Number(processCard.modelData.rssMegabytes)) + " MiB"; font.pixelSize: Kirigami.Theme.smallFont.pixelSize; color: Kirigami.Theme.disabledTextColor }
                            }
                        }
                    }

                    Rectangle {
                        visible: processCard.expanded
                        Layout.fillWidth: true
                        implicitHeight: expDetailsCol.implicitHeight + 12
                        radius: theme.innerRadius
                        color: theme.cardNestedBg

                        ColumnLayout {
                            id: expDetailsCol
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 2
                            QQC2.Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                text: "PID " + processCard.modelData.pid + " · start ticks " + processCard.modelData.startTimeTicks
                                      + " · observed " + (Number(processCard.modelData.observedAtMs) > 0
                                          ? new Date(Number(processCard.modelData.observedAtMs)).toLocaleTimeString() : "Unavailable")
                                      + ". Concurrent resource use does not establish causation."
                            }
                        }
                    }
                }
            }
        }
        MeasurementDetails {
            metadata: psiPage.controller.metricMetadata
        }

    }
}
