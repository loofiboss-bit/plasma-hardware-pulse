pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: psiPage
    objectName: "pressurePage"
    title: "Memory & Pressure"

    required property var controller

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        // Explanation Banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: expCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: 10
            color: Kirigami.Theme.backgroundColor
            border.width: 1
            border.color: Kirigami.Theme.highlightColor

            ColumnLayout {
                id: expCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: 4

                Kirigami.Heading {
                    level: 3
                    text: "Memory & system pressure"
                    color: Kirigami.Theme.textColor
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: "Linux Pressure Stall Information (PSI) measures time tasks spend waiting for CPU, memory, or storage. High RAM use alone does not mean the system is under pressure. The calculated pressure index runs from 100 (little observed pressure) towards 0 (more stalled work)."
                    color: Kirigami.Theme.textColor
                    wrapMode: Text.WordWrap
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: "Calculated pressure index: " + (psiPage.controller.psiAvailable
                          ? psiPage.controller.responsivenessScore.toFixed(1) + "%" : "Unavailable")
                    font.bold: true
                }
            }
        }

        // 3-Axis PSI Radar Cards
        GridLayout {
            Layout.fillWidth: true
            columns: width < 680 ? 1 : 3
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            // CPU PSI
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: cpuPressureColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: 10
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: !psiPage.controller.psiCpuAvailable ? Kirigami.Theme.disabledTextColor
                             : (psiPage.controller.psiCpuSome10 > 10 ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.disabledTextColor)

                ColumnLayout {
                    id: cpuPressureColumn
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    QQC2.Label { text: "CPU pressure"; color: Kirigami.Theme.disabledTextColor; font.pixelSize: Kirigami.Theme.smallFont.pixelSize; font.bold: true }
                    QQC2.Label {
                        text: psiPage.controller.psiCpuAvailable ? ("some: " + psiPage.controller.psiCpuSome10.toFixed(2) + "%  ("
                              + Math.round(psiPage.controller.psiCpuStallMs) + " ms/s)")
                              : "Unavailable"
                        color: Kirigami.Theme.textColor

                        font.bold: true
                    }
                    QQC2.Label {
                        text: "Percentage of time at least one runnable thread is delayed waiting for CPU time."
                        color: Kirigami.Theme.textColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // Memory PSI
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: memoryPressureColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: 10
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: psiPage.controller.psiMemSome10 > 2 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor

                ColumnLayout {
                    id: memoryPressureColumn
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    QQC2.Label { text: "Memory pressure"; color: Kirigami.Theme.disabledTextColor; font.pixelSize: Kirigami.Theme.smallFont.pixelSize; font.bold: true }
                    QQC2.Label {
                        text: psiPage.controller.psiMemoryAvailable
                              ? ("some: " + psiPage.controller.psiMemSome10.toFixed(2) + "% | full: "
                                 + (psiPage.controller.psiMemoryFullAvailable ? psiPage.controller.psiMemFull10.toFixed(2) + "%" : "Unavailable")
                                 + " (" + Math.round(psiPage.controller.psiMemStallMs) + " ms/s)") : "Unavailable"
                        color: Kirigami.Theme.textColor

                        font.bold: true
                    }
                    QQC2.Label {
                        text: "Measures actual thread stall time spent on ZRAM compression, page reclaim, and allocation."
                        color: Kirigami.Theme.textColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // I/O PSI
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: ioPressureColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: 10
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: psiPage.controller.psiIoFull10 > 5 ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.disabledTextColor

                ColumnLayout {
                    id: ioPressureColumn
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    QQC2.Label { text: "Storage and I/O pressure"; color: Kirigami.Theme.disabledTextColor; font.pixelSize: Kirigami.Theme.smallFont.pixelSize; font.bold: true }
                    QQC2.Label {
                        text: psiPage.controller.psiIoAvailable
                              ? ("some: " + psiPage.controller.psiIoSome10.toFixed(2) + "% | full: "
                                 + (psiPage.controller.psiIoFullAvailable ? psiPage.controller.psiIoFull10.toFixed(2) + "%" : "Unavailable")
                                 + " (" + Math.round(psiPage.controller.psiIoStallMs) + " ms/s)") : "Unavailable"
                        color: Kirigami.Theme.textColor

                        font.bold: true
                    }
                    QQC2.Label {
                        visible: psiPage.controller.diskThroughputAvailable
                        text: "Disk throughput: " + psiPage.controller.diskReadMibPerSec.toFixed(1) + " MiB/s read · "
                              + psiPage.controller.diskWriteMibPerSec.toFixed(1) + " MiB/s write"
                        color: Kirigami.Theme.highlightColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        font.bold: true
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: "Indicates when threads are blocked waiting for synchronous NVMe/disk transactions."
                        color: Kirigami.Theme.textColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Visual Memory & ZRAM Pipeline
        Kirigami.Heading {
            level: 3
            text: "Memory use, compression and swap"
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: zramCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: 12
            color: Kirigami.Theme.alternateBackgroundColor
            border.width: 1
            border.color: Kirigami.Theme.alternateBackgroundColor

            ColumnLayout {
                id: zramCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing
                    QQC2.Label {
                        text: psiPage.controller.memoryAvailable ? ("Physical RAM: " + psiPage.controller.ramUsedGib.toFixed(2) + " GiB / "
                              + psiPage.controller.ramTotalGib.toFixed(2) + " GiB ("
                              + psiPage.controller.ramUsedPercent.toFixed(1) + "%)") : "Physical RAM: Unavailable"
                        color: Kirigami.Theme.textColor
                        font.bold: true

                    }
                    QQC2.Label {
                        text: psiPage.controller.swapAvailable
                              ? "Swap used: " + psiPage.controller.swapUsedGib.toFixed(2) + " / "
                                + psiPage.controller.swapTotalGib.toFixed(2) + " GiB"
                              : "Swap: Unavailable"
                        color: Kirigami.Theme.highlightColor
                        font.bold: true

                    }
                    QQC2.Label {
                        text: !psiPage.controller.zramAvailable ? "ZRAM: Unavailable"
                              : (psiPage.controller.zramCompressionAvailable
                                 ? ("ZRAM: " + psiPage.controller.zramCompressionRatio.toFixed(2) + "× compression")
                                 : "ZRAM: No compressed pages in use")
                        color: Kirigami.Theme.positiveTextColor
                        font.bold: true

                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 14
                    radius: 7
                    color: Kirigami.Theme.alternateBackgroundColor

                    Rectangle {
                        visible: psiPage.controller.memoryAvailable
                        width: parent.width * Math.min(1.0, Math.max(0, psiPage.controller.ramUsedPercent / 100.0))
                        height: parent.height
                        radius: 7
                        color: Kirigami.Theme.highlightColor
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    text: !psiPage.controller.zramAvailable ? "ZRAM details: Unavailable"
                          : ("ZRAM data: " + Math.round(psiPage.controller.zramOrigMib) + " MiB before compression, "
                             + Math.round(psiPage.controller.zramComprMib) + " MiB compressed payload and "
                             + Math.round(psiPage.controller.zramMemUsedMib) + " MiB allocator memory.")
                    color: Kirigami.Theme.textColor
                    wrapMode: Text.WordWrap
                }
            }
        }

        // Processes are contextual workload observations, not proof of causality.
        Kirigami.Heading {
            level: 3
            text: "Concurrent resource use"
        }

        Repeater {
            model: psiPage.controller.topProcesses

            delegate: Rectangle {
                id: processCard
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: processContent.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: 8
                color: Kirigami.Theme.alternateBackgroundColor

                ColumnLayout {
                    id: processContent
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: processCard.modelData.name + " (PID " + processCard.modelData.pid + ") · concurrent workload"
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: "Observed use: " + Number(processCard.modelData.cpuPercent).toFixed(1) + "% CPU · "
                              + Math.round(Number(processCard.modelData.rssMegabytes)) + " MiB RSS"
                        wrapMode: Text.WordWrap
                    }
                    QQC2.Button {
                        icon.name: "utilities-system-monitor"
                        text: "Open System Monitor"
                        onClicked: psiPage.controller.openSystemMonitor()
                    }
                }
            }
        }
    }
}
