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
    id: gpuPage
    objectName: "gpuPage"
    title: KI18n.i18nd("loofi-pulse", "Graphics (GPU)")
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None
    titleDelegate: Item {
        width: parent ? parent.width : 0
        height: 0
        visible: false
    }
    topPadding: Kirigami.Units.largeSpacing
    bottomPadding: Kirigami.Units.largeSpacing
    leftPadding: Kirigami.Units.largeSpacing
    rightPadding: Kirigami.Units.largeSpacing

    required property PulseTelemetryController controller

    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: gpuPage.Kirigami.Theme.backgroundColor
        disabledColor: gpuPage.Kirigami.Theme.disabledTextColor
        negativeColor: gpuPage.Kirigami.Theme.negativeTextColor
        neutralColor: gpuPage.Kirigami.Theme.neutralTextColor
        positiveColor: gpuPage.Kirigami.Theme.positiveTextColor
        textColor: gpuPage.Kirigami.Theme.textColor
        highlightColor: gpuPage.Kirigami.Theme.highlightColor
    }

    readonly property bool hasGpu: controller.gpuAvailable && ((controller.gpuDevices && controller.gpuDevices.length > 0) || controller.gpuBusyAvailable)

    function formatTemp(celsius) {
        return MeasurementLabels.formatTemperature(celsius, gpuPage.controller.temperatureUnit)
    }

    ColumnLayout {
        width: gpuPage.width - Kirigami.Units.largeSpacing * 2
        spacing: Kirigami.Units.largeSpacing

        // ==========================================
        // 1. PAGE HEADER STRIP
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            spacing: gpuPage.theme.spacingM

            ColumnLayout {
                spacing: 2
                QQC2.Label {
                    text: KI18n.i18nd("loofi-pulse", "Graphics (GPU)")
                    font.weight: Font.DemiBold
                    font.pointSize: gpuPage.theme.fontHeadline
                    color: gpuPage.theme.textColor
                }
                QQC2.Label {
                    text: KI18n.i18nd("loofi-pulse", "GPU load, clock frequencies, VRAM consumption, temperature and power in real time.")
                    font.pointSize: gpuPage.theme.fontCaption
                    color: gpuPage.theme.mutedTextColor
                }
            }

            Item { Layout.fillWidth: true }

            // Active GPU Status Badge
            Rectangle {
                radius: gpuPage.theme.pillRadius
                color: gpuPage.theme.cardNestedBg
                border.width: 1
                border.color: gpuPage.theme.subtleBorderColor
                implicitHeight: 26
                implicitWidth: gpuStatusRow.implicitWidth + 18

                RowLayout {
                    id: gpuStatusRow
                    anchors.centerIn: parent
                    spacing: 6
                    Kirigami.Icon {
                        source: "video-display"
                        implicitWidth: 12
                        implicitHeight: 12
                        color: gpuPage.theme.gpuAccent
                    }
                    QQC2.Label {
                        text: gpuPage.hasGpu ? (controller.gpuDriverName ? controller.gpuDriverName : KI18n.i18n("GPU Active")) : KI18n.i18n("No GPU detected")
                        font.bold: true
                        font.pointSize: gpuPage.theme.fontCaption
                        color: gpuPage.hasGpu ? gpuPage.theme.gpuAccent : gpuPage.theme.faintTextColor
                    }
                }
            }
        }

        // ==========================================
        // 2. PRIMARY GPU TELEMETRY HERO CARD
        // ==========================================
        PulseCard {
            Layout.fillWidth: true
            padding: theme.spacingL
            accentColor: theme.gpuAccent
            accentStrength: 0.12

            ColumnLayout {
                id: heroCol
                objectName: "section_gpu"
                focus: true
                Accessible.name: KI18n.i18n("gpu section")
                width: parent.width
                spacing: Kirigami.Units.mediumSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        radius: 6
                        color: theme.accentTint(theme.gpuAccent, theme.isDark ? 0.22 : 0.14)
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            source: "video-display"
                            color: theme.gpuAccent
                        }
                    }
                    ColumnLayout {
                        spacing: 1
                        QQC2.Label {
                            text: gpuPage.hasGpu
                                  ? (controller.selectedGpuDeviceName || (controller.gpuDriverName ? controller.gpuDriverName.toUpperCase() + " GPU" : "Graphics Adapter"))
                                  : KI18n.i18n("No GPU detected")
                            font.weight: Font.DemiBold
                            font.pointSize: gpuPage.theme.fontTitle
                            color: gpuPage.theme.textColor
                        }
                        QQC2.Label {
                            text: KI18n.i18n("Primary rendering & display controller")
                            font.pointSize: gpuPage.theme.fontCaption
                            color: gpuPage.theme.mutedTextColor
                        }
                    }
                    Item { Layout.fillWidth: true }
                    QQC2.Label {
                        visible: gpuPage.hasGpu && controller.gpuBusyAvailable
                        text: Math.round(controller.gpuBusyPercent) + "%"
                        font.weight: Font.Bold
                        font.pointSize: gpuPage.theme.fontHeadline * 1.3
                        color: theme.gpuAccent
                    }
                }

                // Busy Load Gauge Bar
                Rectangle {
                    visible: gpuPage.hasGpu
                    Layout.fillWidth: true
                    Layout.preferredHeight: 10
                    radius: 5
                    color: theme.trackColor
                    Rectangle {
                        visible: controller.gpuBusyAvailable
                        width: parent.width * Math.min(1.0, Math.max(0.0, controller.gpuBusyPercent / 100.0))
                        height: parent.height
                        radius: 5
                        color: theme.gpuAccent
                    }
                }

                // Metric Pills Grid
                GridLayout {
                    Layout.fillWidth: true
                    columns: gpuPage.width < 680 ? 2 : 4
                    columnSpacing: Kirigami.Units.mediumSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    // Temperature
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: tempCol.implicitHeight + 14
                        radius: theme.innerRadius
                        color: theme.cardNestedBg
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        ColumnLayout {
                            id: tempCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            QQC2.Label {
                                text: KI18n.i18n("Temperature")
                                font.pointSize: gpuPage.theme.fontCaption
                                color: gpuPage.theme.mutedTextColor
                            }
                            QQC2.Label {
                                text: controller.gpuTempAvailable ? gpuPage.formatTemp(controller.gpuTempCelsius) : KI18n.i18n("Unavailable")
                                font.weight: Font.DemiBold
                                font.pointSize: gpuPage.theme.fontBody * 1.15
                                color: controller.gpuTempAvailable && controller.gpuTempCelsius > 80 ? theme.negativeColor : gpuPage.theme.textColor
                            }
                        }
                    }

                    // Frequency
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: freqCol.implicitHeight + 14
                        radius: theme.innerRadius
                        color: theme.cardNestedBg
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        ColumnLayout {
                            id: freqCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            QQC2.Label {
                                text: KI18n.i18n("Clock Speed")
                                font.pointSize: gpuPage.theme.fontCaption
                                color: gpuPage.theme.mutedTextColor
                            }
                            QQC2.Label {
                                text: controller.gpuFrequencyAvailable ? (controller.gpuCurrentFreqMhz + " MHz") : KI18n.i18n("Unavailable")
                                font.weight: Font.DemiBold
                                font.pointSize: gpuPage.theme.fontBody * 1.15
                                color: gpuPage.theme.textColor
                            }
                        }
                    }

                    // Power Draw
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: pwrCol.implicitHeight + 14
                        radius: theme.innerRadius
                        color: theme.cardNestedBg
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        ColumnLayout {
                            id: pwrCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            QQC2.Label {
                                text: KI18n.i18n("Power Draw")
                                font.pointSize: gpuPage.theme.fontCaption
                                color: gpuPage.theme.mutedTextColor
                            }
                            QQC2.Label {
                                text: controller.gpuPowerAvailable ? (controller.gpuPowerWatts.toFixed(1) + " W") : KI18n.i18n("Unavailable")
                                font.weight: Font.DemiBold
                                font.pointSize: gpuPage.theme.fontBody * 1.15
                                color: gpuPage.theme.textColor
                            }
                        }
                    }

                    // Driver
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: drvCol.implicitHeight + 14
                        radius: theme.innerRadius
                        color: theme.cardNestedBg
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        ColumnLayout {
                            id: drvCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            QQC2.Label {
                                text: KI18n.i18n("Kernel Driver")
                                font.pointSize: gpuPage.theme.fontCaption
                                color: gpuPage.theme.mutedTextColor
                            }
                            QQC2.Label {
                                text: controller.gpuDriverName ? controller.gpuDriverName : KI18n.i18n("Generic")
                                font.weight: Font.DemiBold
                                font.pointSize: gpuPage.theme.fontBody * 1.15
                                color: gpuPage.theme.textColor
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 3. VRAM & VIDEO MEMORY CARD
        // ==========================================
        PulseCard {
            Layout.fillWidth: true
            padding: theme.spacingL

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Kirigami.Icon {
                        source: "drive-multidisk"
                        implicitWidth: 16
                        implicitHeight: 16
                        color: theme.gpuAccent
                    }
                    Kirigami.Heading {
                        level: 3
                        text: KI18n.i18n("Video Memory (VRAM)")
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: controller.gpuVramAvailable
                              ? Math.round(controller.gpuVramUsedMib) + " / " + Math.round(controller.gpuVramTotalMib) + " MiB"
                              : KI18n.i18n("Unavailable or Unified")
                        font.weight: Font.DemiBold
                        color: gpuPage.theme.textColor
                    }
                }

                Rectangle {
                    visible: controller.gpuVramAvailable
                    Layout.fillWidth: true
                    Layout.preferredHeight: 8
                    radius: 4
                    color: theme.trackColor
                    Rectangle {
                        width: parent.width * Math.min(1.0, Math.max(0.0, controller.gpuVramTotalMib > 0 ? (controller.gpuVramUsedMib / controller.gpuVramTotalMib) : 0.0))
                        height: parent.height
                        radius: 4
                        color: theme.gpuAccent
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pointSize: gpuPage.theme.fontCaption
                    color: gpuPage.theme.mutedTextColor
                    text: controller.gpuVramAvailable
                          ? KI18n.i18n("Dedicated video memory allocated for framebuffers, textures and compute pipelines.")
                          : KI18n.i18n("This GPU uses unified system memory architecture (UMA) shared dynamically with host RAM.")
                }
            }
        }

        // ==========================================
        // 4. DETECTED GRAPHICS ADAPTERS LIST
        // ==========================================
        PulseCard {
            Layout.fillWidth: true
            padding: theme.spacingL

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Kirigami.Icon {
                        source: "video-display"
                        implicitWidth: 16
                        implicitHeight: 16
                        color: theme.highlightColor
                    }
                    Kirigami.Heading {
                        level: 3
                        text: KI18n.i18n("Installed Graphics Adapters")
                        Layout.fillWidth: true
                    }
                }

                QQC2.Label {
                    visible: (controller.gpuDevices || []).length === 0
                    text: KI18n.i18n("No supported GPU telemetry interface found")
                    color: Kirigami.Theme.disabledTextColor
                }

                Repeater {
                    model: controller.gpuDevices || []
                    delegate: Rectangle {
                        id: gpuDeviceItem
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: devRow.implicitHeight + 14
                        radius: theme.innerRadius
                        color: controller.selectedGpuId === modelData.id ? theme.selectionColor : theme.cardNestedBg
                        border.width: 1
                        border.color: controller.selectedGpuId === modelData.id ? theme.highlightColor : theme.subtleBorderColor

                        RowLayout {
                            id: devRow
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10

                            QQC2.RadioButton {
                                checked: controller.selectedGpuId === gpuDeviceItem.modelData.id
                                onClicked: controller.selectedGpuId = gpuDeviceItem.modelData.id
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                QQC2.Label {
                                    text: gpuDeviceItem.modelData.name || "GPU Device"
                                    font.weight: Font.DemiBold
                                    color: gpuPage.theme.textColor
                                }
                                QQC2.Label {
                                    text: (gpuDeviceItem.modelData.busyAvailable ? Number(gpuDeviceItem.modelData.busyPercent).toFixed(0) + "% load" : "Load N/A")
                                          + " · " + (gpuDeviceItem.modelData.temperatureAvailable ? gpuPage.formatTemp(gpuDeviceItem.modelData.tempCelsius) : "Temp N/A")
                                          + (gpuDeviceItem.modelData.available ? "" : " · Disconnected")
                                    font.pointSize: gpuPage.theme.fontCaption
                                    color: gpuPage.theme.mutedTextColor
                                }
                            }

                            Rectangle {
                                radius: 4
                                color: controller.selectedGpuId === gpuDeviceItem.modelData.id ? theme.highlightColor : theme.subtleBorderColor
                                implicitHeight: 20
                                implicitWidth: selLbl.implicitWidth + 12
                                QQC2.Label {
                                    id: selLbl
                                    anchors.centerIn: parent
                                    text: controller.selectedGpuId === gpuDeviceItem.modelData.id ? KI18n.i18n("Active") : KI18n.i18n("Select")
                                    font.pointSize: gpuPage.theme.fontCaption * 0.9
                                    font.weight: Font.DemiBold
                                    color: controller.selectedGpuId === gpuDeviceItem.modelData.id ? "#ffffff" : gpuPage.theme.mutedTextColor
                                }
                            }
                        }

                        HoverHandler { id: devHov; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            onTapped: controller.selectedGpuId = gpuDeviceItem.modelData.id
                        }
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pointSize: gpuPage.theme.fontCaption
                    color: gpuPage.theme.faintTextColor
                    text: KI18n.i18n("Select a GPU for the primary live summary and recorded history. Availability depends on device drivers.")
                }
            }
        }
    }
}
