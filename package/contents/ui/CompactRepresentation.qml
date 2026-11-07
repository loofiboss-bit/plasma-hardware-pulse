pragma ComponentBehavior: Bound

import QtQuick
import org.kde.ki18n
import "TemperatureLabels.js" as TemperatureLabels
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

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
    readonly property bool motionAllowed: Kirigami.Units.shortDuration > 0 && Kirigami.Units.longDuration > 0

    readonly property PulseTheme theme: PulseTheme {
        id: pulseTheme
        bgColor: Kirigami.Theme.backgroundColor
        textColor: Kirigami.Theme.textColor
        highlightColor: Kirigami.Theme.highlightColor
        disabledColor: Kirigami.Theme.disabledTextColor
        negativeColor: Kirigami.Theme.negativeTextColor
        neutralColor: Kirigami.Theme.neutralTextColor
        positiveColor: Kirigami.Theme.positiveTextColor
    }

    function recentCpuValues() {
        var rawJson = compactRoot.controller.historyRecentSeriesJson("cpuLoadPercent", "cpuLoadAvailable", 16)
        if (!rawJson) return []
        try {
            var samples = JSON.parse(rawJson)
            var res = []
            for (var i = 0; i < samples.length; ++i) {
                res.push(samples[i].available && isFinite(samples[i].value) ? samples[i].value : 0)
            }
            return res
        } catch (e) {
            return []
        }
    }

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

                // Style 0, 1 & 2: Clean Breeze status dot (no animations or halos in v0.16)
                Item {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8

                    Rectangle {
                        anchors.centerIn: parent
                        width: 7
                        height: 7
                        radius: 3.5
                        color: compactRoot.badgeColor(compactRoot.controller.verdictSeverity)
                    }
                }

                // Style 1: Mini Sparkline
                Item {
                    id: miniSparkWrapper
                    visible: compactRoot.panelDisplayStyle === 1
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: compactRoot.verticalPanel ? 26 : 38
                    Layout.preferredHeight: 14

                    PulseSparkline {
                        anchors.fill: parent
                        values: compactRoot.recentCpuValues()
                        color: compactRoot.badgeColor(compactRoot.controller.verdictSeverity)
                        minValue: 0
                        maxValue: 100
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
