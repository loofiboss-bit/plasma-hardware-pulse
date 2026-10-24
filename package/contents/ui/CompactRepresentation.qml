pragma ComponentBehavior: Bound

import QtQuick
import org.kde.ki18n
import "TemperatureLabels.js" as TemperatureLabels
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

Item {
    id: compactRoot

    required property var controller
    required property bool expanded
    property bool verticalPanel: false
    property int panelDisplayStyle: 0
    property bool showCpuLoadInPanel: false
    property bool showGpuInPanel: false
    property bool showRamInPanel: false
    property bool showIoInPanel: false
    property bool showPsiInPanel: true
    property bool showWattsInPanel: true
    property bool alwaysShowStatusInPanel: false
    signal activationRequested()

    readonly property string temperatureText: controller.cpuTempAvailable
        ? TemperatureLabels.formatTemperature(controller.cpuPackageTemp, controller.temperatureUnit) : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Temp N/A")
    readonly property string panelText: {
        let values = [temperatureText]
        if (alwaysShowStatusInPanel || (controller.verdictSeverity >= 1 && controller.verdictSeverity <= 3))
            values.push(KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", controller.verdictHeadline))
        if (showPsiInPanel)
            values.push(controller.psiAvailable ? KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Responsiveness ") + Math.round(controller.responsivenessScore) + "/100" : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Responsiveness N/A"))
        if (showCpuLoadInPanel)
            values.push(controller.cpuLoadAvailable ? Math.round(controller.cpuLoadPercent) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "% CPU") : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU N/A"))
        if (showGpuInPanel)
            values.push(controller.gpuBusyAvailable ? Math.round(controller.gpuBusyPercent) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "% GPU")
                        : (controller.gpuTempAvailable ? TemperatureLabels.formatTemperature(controller.gpuTempCelsius, controller.temperatureUnit) + " GPU" : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "GPU N/A")))
        if (showRamInPanel)
            values.push(controller.memoryAvailable ? Math.round(controller.ramUsedPercent) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "% RAM") : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "RAM N/A"))
        if (showIoInPanel)
            values.push(controller.diskThroughputAvailable
                        ? Number(controller.diskReadMibPerSec + controller.diskWriteMibPerSec).toFixed(1) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " MiB/s disk")
                        : controller.netThroughputAvailable ? Number(controller.netRxMibPerSec + controller.netTxMibPerSec).toFixed(1) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " MiB/s net") : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "I/O N/A"))
        if (showWattsInPanel)
            values.push(controller.packagePowerAvailable
                        ? controller.packageWatts.toFixed(1) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " W CPU package") + (controller.packagePowerEstimated ? KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " (est.)") : "") : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU package W N/A"))
        return values.join(" · ")
    }

    Layout.minimumWidth: verticalPanel ? Kirigami.Units.gridUnit * 2 : (panelDisplayStyle === 1 ? Kirigami.Units.gridUnit * 6 : Kirigami.Units.gridUnit * 4)
    Layout.preferredWidth: verticalPanel ? Kirigami.Units.gridUnit * 2.5
                          : Math.min(panelRow.implicitWidth + Kirigami.Units.largeSpacing * 2, Kirigami.Units.gridUnit * 20)
    Layout.maximumWidth: verticalPanel ? -1 : Kirigami.Units.gridUnit * 20
    Layout.minimumHeight: Kirigami.Units.iconSizes.smallMedium
    Layout.preferredHeight: verticalPanel ? Kirigami.Units.gridUnit * 2.5 : Kirigami.Units.iconSizes.smallMedium

    function badgeColor(severity) {
        if (severity === 4) return Kirigami.Theme.disabledTextColor
        if (severity === 3 || severity === 2) return Kirigami.Theme.negativeTextColor
        if (severity === 1) return Kirigami.Theme.neutralTextColor
        return Kirigami.Theme.positiveTextColor
    }

    PlasmaComponents3.Button {
        id: pillButton
        objectName: "panelStatusButton"
        anchors.fill: parent
        anchors.margins: 2
        checkable: true
        checked: compactRoot.expanded
        Accessible.name: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", compactRoot.controller.verdictHeadline) + ". " + compactRoot.panelText
        Accessible.description: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Activate to open hardware details. Higher responsiveness is better. Additional current readings are available in the tooltip.")
        onClicked: compactRoot.activationRequested()

        contentItem: Item {
            clip: true
            RowLayout {
                id: panelRow
                anchors.fill: parent
                spacing: Kirigami.Units.smallSpacing

                // Style 0 & 1: Status dot
                Rectangle {
                    visible: compactRoot.panelDisplayStyle !== 2
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    radius: 3.5
                    color: compactRoot.badgeColor(compactRoot.controller.verdictSeverity)
                }

                // Style 2: Ring gauge
                Item {
                    id: ringGaugeWrapper
                    visible: compactRoot.panelDisplayStyle === 2
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16

                    Canvas {
                        id: ringCanvas
                        anchors.fill: parent
                        renderTarget: Canvas.Image
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var cx = width / 2
                            var cy = height / 2
                            var r = Math.min(cx, cy) - 2
                            if (r <= 0) return

                            // Track
                            ctx.beginPath()
                            ctx.arc(cx, cy, r, 0, 2 * Math.PI)
                            ctx.lineWidth = 2.5
                            ctx.strokeStyle = Kirigami.Theme.disabledTextColor
                            ctx.globalAlpha = 0.3
                            ctx.stroke()
                            ctx.globalAlpha = 1.0

                            // Value arc (CPU load)
                            var load = compactRoot.controller.cpuLoadAvailable ? (compactRoot.controller.cpuLoadPercent / 100.0) : 0.0
                            var fraction = Math.min(1.0, Math.max(0.0, load))
                            if (fraction > 0.02) {
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, -Math.PI / 2, -Math.PI / 2 + fraction * 2 * Math.PI)
                                ctx.lineWidth = 2.5
                                ctx.strokeStyle = compactRoot.badgeColor(compactRoot.controller.verdictSeverity)
                                ctx.stroke()
                            }
                        }
                        Connections {
                            target: compactRoot.controller
                            function onTelemetryUpdated() {
                                if (compactRoot.panelDisplayStyle === 2) ringCanvas.requestPaint()
                            }
                        }
                    }
                }

                // Style 1: Mini Sparkline
                Item {
                    id: miniSparkWrapper
                    visible: compactRoot.panelDisplayStyle === 1
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: compactRoot.verticalPanel ? 26 : 38
                    Layout.preferredHeight: 14

                    Canvas {
                        id: sparkCanvas
                        anchors.fill: parent
                        renderTarget: Canvas.Image
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var rawJson = compactRoot.controller.historyRecentSeriesJson("cpuLoadPercent", "cpuLoadAvailable", 16)
                            if (!rawJson) return
                            var samples = []
                            try {
                                samples = JSON.parse(rawJson)
                            } catch (e) {
                                return
                            }
                            var count = samples.length
                            if (count < 2) return

                            var pts = []
                            for (var i = 0; i < count; ++i) {
                                var s = samples[i]
                                if (!s.available || !isFinite(s.value)) { pts.push(null); continue }
                                var px = (i / Math.max(1, count - 1)) * width
                                var norm = Math.min(1.0, Math.max(0.0, s.value / 100.0))
                                var py = height - norm * (height - 3) - 1.5
                                pts.push({ x: px, y: py })
                            }

                            var strokeColor = compactRoot.badgeColor(compactRoot.controller.verdictSeverity)
                            ctx.strokeStyle = strokeColor
                            ctx.lineWidth = 1.5
                            ctx.beginPath()
                            var active = false
                            for (var j = 0; j < pts.length; ++j) {
                                var pt = pts[j]
                                if (!pt) { active = false; continue }
                                if (!active) {
                                    ctx.moveTo(pt.x, pt.y)
                                    active = true
                                } else {
                                    ctx.lineTo(pt.x, pt.y)
                                }
                            }
                            ctx.stroke()
                        }
                        Connections {
                            target: compactRoot.controller
                            function onTelemetryUpdated() {
                                if (compactRoot.panelDisplayStyle === 1) sparkCanvas.requestPaint()
                            }
                        }
                    }
                }

                PlasmaComponents3.Label {
                    id: panelLabel
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: compactRoot.verticalPanel ? compactRoot.temperatureText : compactRoot.panelText
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
            }
        }
    }
}
