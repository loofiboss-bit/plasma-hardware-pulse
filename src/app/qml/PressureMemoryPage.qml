import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: psiPage
    title: "Linux PSI Pressure & ZRAM Memory Pipeline"

    required property var controller

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        // Explanation Banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: expCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: 10
            color: Qt.rgba(0.08, 0.13, 0.22, 0.9)
            border.width: 1
            border.color: "#38bdf8"

            ColumnLayout {
                id: expCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: 4

                Kirigami.Heading {
                    level: 3
                    text: "Why Linux PSI (Pressure Stall Information) Is More Logical Than Raw RAM %"
                    color: "#f8fafc"
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: "Linux uses free RAM as disk page cache. High RAM utilization with 0.00% Memory PSI means the system is healthy. When PSI 'some' or 'full' rises above 0%, threads are actively stalling on ZRAM compaction, page reclaim, or disk I/O."
                    color: "#cbd5e1"
                    wrapMode: Text.WordWrap
                }
            }
        }

        // 3-Axis PSI Radar Cards
        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            // CPU PSI
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 130
                radius: 10
                color: Qt.rgba(0.11, 0.15, 0.24, 0.88)
                border.width: 1
                border.color: psiPage.controller.psiCpuSome10 > 10 ? "#f59e0b" : Qt.rgba(1, 1, 1, 0.12)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    QQC2.Label { text: "CPU PRESSURE (/proc/pressure/cpu)"; color: "#94a3b8"; font.pixelSize: 11; font.bold: true }
                    QQC2.Label {
                        text: "some: " + psiPage.controller.psiCpuSome10.toFixed(2) + "%  ("
                              + Math.round(psiPage.controller.psiCpuStallMs) + " ms/s)"
                        color: "#f8fafc"
                        font.pixelSize: 19
                        font.bold: true
                    }
                    QQC2.Label {
                        text: "Percentage of time at least one runnable thread is delayed waiting for CPU time."
                        color: "#cbd5e1"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // Memory PSI
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 130
                radius: 10
                color: Qt.rgba(0.11, 0.15, 0.24, 0.88)
                border.width: 1
                border.color: psiPage.controller.psiMemSome10 > 2 ? "#ef4444" : Qt.rgba(1, 1, 1, 0.12)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    QQC2.Label { text: "MEMORY PRESSURE (/proc/pressure/memory)"; color: "#94a3b8"; font.pixelSize: 11; font.bold: true }
                    QQC2.Label {
                        text: "some: " + psiPage.controller.psiMemSome10.toFixed(2)
                              + "% | full: " + psiPage.controller.psiMemFull10.toFixed(2)
                              + "% (" + Math.round(psiPage.controller.psiMemStallMs) + " ms/s)"
                        color: "#f8fafc"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    QQC2.Label {
                        text: "Measures actual thread stall time spent on ZRAM compression, page reclaim, and allocation."
                        color: "#cbd5e1"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // I/O PSI
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 130
                radius: 10
                color: Qt.rgba(0.11, 0.15, 0.24, 0.88)
                border.width: 1
                border.color: psiPage.controller.psiIoFull10 > 5 ? "#f97316" : Qt.rgba(1, 1, 1, 0.12)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    QQC2.Label { text: "I/O PRESSURE (/proc/pressure/io)"; color: "#94a3b8"; font.pixelSize: 11; font.bold: true }
                    QQC2.Label {
                        text: "some: " + psiPage.controller.psiIoSome10.toFixed(2)
                              + "% | full: " + psiPage.controller.psiIoFull10.toFixed(2)
                              + "% (" + Math.round(psiPage.controller.psiIoStallMs) + " ms/s)"
                        color: "#f8fafc"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    QQC2.Label {
                        text: "Indicates when threads are blocked waiting for synchronous NVMe/disk transactions."
                        color: "#cbd5e1"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Visual Memory & ZRAM Pipeline
        Kirigami.Heading {
            level: 3
            text: "Logical Memory Pipeline: Physical RAM ➔ ZRAM Compression Engine ➔ Swap"
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: zramCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: 12
            color: Qt.rgba(0.09, 0.13, 0.21, 0.90)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.15)

            ColumnLayout {
                id: zramCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                RowLayout {
                    Layout.fillWidth: true
                    QQC2.Label {
                        text: "Physical RAM: " + psiPage.controller.ramUsedGib.toFixed(2) + " GiB / "
                              + psiPage.controller.ramTotalGib.toFixed(2) + " GiB ("
                              + psiPage.controller.ramUsedPercent.toFixed(1) + "%)"
                        color: "#f8fafc"
                        font.bold: true
                        font.pixelSize: 16
                    }
                    Item { Layout.fillWidth: true }
                    QQC2.Label {
                        text: "Swap Used: " + psiPage.controller.swapUsedGib.toFixed(2) + " / "
                              + psiPage.controller.swapTotalGib.toFixed(2) + " GiB"
                        color: "#38bdf8"
                        font.bold: true
                        font.pixelSize: 15
                    }
                    Item { Layout.preferredWidth: 16 }
                    QQC2.Label {
                        text: psiPage.controller.zramActive
                              ? ("ZRAM Efficiency: " + psiPage.controller.zramCompressionRatio.toFixed(2) + "× compression")
                              : "ZRAM inactive"
                        color: "#10b981"
                        font.bold: true
                        font.pixelSize: 16
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 14
                    radius: 7
                    color: Qt.rgba(1, 1, 1, 0.12)

                    Rectangle {
                        width: parent.width * Math.min(1.0, Math.max(0.02, psiPage.controller.ramUsedPercent / 100.0))
                        height: parent.height
                        radius: 7
                        color: "#38bdf8"
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    text: "ZRAM (/sys/block/zram0/mm_stat): Uncompressed data "
                          + Math.round(psiPage.controller.zramOrigMib) + " MiB compressed down to "
                          + Math.round(psiPage.controller.zramComprMib) + " MiB payload ("
                          + Math.round(psiPage.controller.zramMemUsedMib) + " MiB total allocator RAM used • Physical RAM saved: "
                          + Math.max(0, Math.round(psiPage.controller.zramOrigMib - psiPage.controller.zramComprMib)) + " MiB)."
                    color: "#cbd5e1"
                    wrapMode: Text.WordWrap
                }
            }
        }

        // Top Resource Processes (CPU & RAM/RSS Culprits) with Terminate Action
        Kirigami.Heading {
            level: 3
            text: "Top Resource Culprits (Live CPU & Memory RSS Radar)"
        }

        Repeater {
            model: psiPage.controller.topProcesses

            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 54
                radius: 8
                color: Qt.rgba(0.10, 0.14, 0.23, 0.85)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Kirigami.Units.largeSpacing
                    anchors.rightMargin: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.largeSpacing

                    QQC2.Label {
                        Layout.preferredWidth: 260
                        text: modelData.name + "  (PID " + modelData.pid + ")"
                        color: "#f8fafc"
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    QQC2.Label {
                        Layout.preferredWidth: 140
                        text: "CPU: " + Number(modelData.cpuPercent || 0).toFixed(1) + "%"
                        color: Number(modelData.cpuPercent || 0) >= 25.0 ? "#f59e0b" : "#38bdf8"
                        font.bold: true
                        font.family: "monospace"
                    }

                    QQC2.Label {
                        Layout.preferredWidth: 180
                        text: "Memory RSS: " + Math.round(Number(modelData.rssMegabytes || 0)) + " MiB"
                        color: "#cbd5e1"
                        font.family: "monospace"
                    }

                    Item { Layout.fillWidth: true }

                    QQC2.Button {
                        icon.name: "process-stop"
                        text: "Terminate (SIGTERM)"
                        onClicked: psiPage.controller.terminateProcess(modelData.pid, modelData.name)
                    }
                }
            }
        }
    }
}
