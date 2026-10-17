pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

Item {
    id: compactRoot

    required property var controller
    required property bool expanded
    property bool verticalPanel: false
    property bool showCpuLoadInPanel: false
    property bool showGpuInPanel: false
    property bool showRamInPanel: false
    property bool showIoInPanel: false
    property bool showPsiInPanel: true
    property bool showWattsInPanel: true
    property bool alwaysShowStatusInPanel: false
    signal activationRequested()

    readonly property string temperatureText: controller.cpuTempAvailable
        ? Math.round(controller.cpuPackageTemp) + "°C" : "Temp N/A"
    readonly property string panelText: {
        let values = [temperatureText]
        if (alwaysShowStatusInPanel || (controller.verdictSeverity >= 1 && controller.verdictSeverity <= 3))
            values.push(controller.verdictHeadline)
        if (showPsiInPanel)
            values.push(controller.psiAvailable ? "Responsiveness " + Math.round(controller.responsivenessScore) + "/100" : "Responsiveness N/A")
        if (showCpuLoadInPanel)
            values.push(controller.cpuLoadAvailable ? Math.round(controller.cpuLoadPercent) + "% CPU" : "CPU N/A")
        if (showGpuInPanel)
            values.push(controller.gpuBusyAvailable ? Math.round(controller.gpuBusyPercent) + "% GPU"
                        : (controller.gpuTempAvailable ? Math.round(controller.gpuTempCelsius) + "°C GPU" : "GPU N/A"))
        if (showRamInPanel)
            values.push(controller.memoryAvailable ? Math.round(controller.ramUsedPercent) + "% RAM" : "RAM N/A")
        if (showIoInPanel)
            values.push(controller.diskThroughputAvailable
                        ? Number(controller.diskReadMibPerSec + controller.diskWriteMibPerSec).toFixed(1) + " MiB/s disk"
                        : controller.netThroughputAvailable ? Number(controller.netRxMibPerSec + controller.netTxMibPerSec).toFixed(1) + " MiB/s net" : "I/O N/A")
        if (showWattsInPanel)
            values.push(controller.packagePowerAvailable
                        ? controller.packageWatts.toFixed(1) + " W CPU package" + (controller.packagePowerEstimated ? " (est.)" : "") : "CPU package W N/A")
        return values.join(" · ")
    }

    Layout.minimumWidth: verticalPanel ? Kirigami.Units.gridUnit * 2 : Kirigami.Units.gridUnit * 4
    Layout.preferredWidth: verticalPanel ? Kirigami.Units.gridUnit * 2.5
                          : Math.min(panelLabel.implicitWidth + Kirigami.Units.largeSpacing * 3, Kirigami.Units.gridUnit * 18)
    Layout.maximumWidth: verticalPanel ? -1 : Kirigami.Units.gridUnit * 18
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
        Accessible.name: compactRoot.controller.verdictHeadline + ". " + compactRoot.panelText
        Accessible.description: "Activate to open hardware details. Higher responsiveness is better. Additional current readings are available in the tooltip."
        onClicked: compactRoot.activationRequested()

        contentItem: Item {
            clip: true
            RowLayout {
                anchors.fill: parent
                spacing: Kirigami.Units.smallSpacing
                Rectangle {
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    radius: 3.5
                    color: compactRoot.badgeColor(compactRoot.controller.verdictSeverity)
                }
                PlasmaComponents3.Label {
                    id: panelLabel
                    Layout.fillWidth: true
                    text: compactRoot.verticalPanel ? compactRoot.temperatureText : compactRoot.panelText
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }
            }
        }
    }
}
