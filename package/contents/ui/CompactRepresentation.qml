pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

Item {
    id: compactRoot

    required property var controller
    required property bool expanded
    property bool showPsiInPanel: true
    property bool showWattsInPanel: true
    property bool alwaysShowStatusInPanel: false
    signal activationRequested()

    Layout.minimumWidth: pillRow.implicitWidth + Kirigami.Units.smallSpacing * 4
    Layout.preferredWidth: Layout.minimumWidth
    Layout.minimumHeight: Kirigami.Units.iconSizes.small

    function badgeColor(sev) {
        if (sev === 4) return Kirigami.Theme.disabledTextColor
        if (sev === 3) return Kirigami.Theme.negativeTextColor
        if (sev === 2) return Kirigami.Theme.negativeTextColor
        if (sev === 1) return Kirigami.Theme.neutralTextColor
        return Kirigami.Theme.positiveTextColor
    }

    function velocityArrow(vel) {
        if (vel >= 0.4) return "↑"
        if (vel >= 0.15) return "↗"
        if (vel <= -0.4) return "↓"
        if (vel <= -0.15) return "↘"
        return "•"
    }

    PlasmaComponents3.Button {
        id: pillRow
        anchors.fill: parent
        anchors.margins: 2
        checkable: true
        checked: compactRoot.expanded
        Accessible.name: compactRoot.controller.verdictHeadline + ". Activate to open hardware details."
        Accessible.description: "Loofi Pulse status and current hardware readings."
        onClicked: compactRoot.activationRequested()
        contentItem: RowLayout {
            spacing: Kirigami.Units.smallSpacing

            Rectangle {
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                radius: 4
                color: compactRoot.badgeColor(compactRoot.controller.verdictSeverity)
            }

            PlasmaComponents3.Label {
                visible: compactRoot.alwaysShowStatusInPanel
                         || (compactRoot.controller.verdictSeverity >= 1 && compactRoot.controller.verdictSeverity <= 3)
                text: compactRoot.controller.verdictHeadline
                elide: Text.ElideRight
                Layout.maximumWidth: Kirigami.Units.gridUnit * 9
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                font.bold: compactRoot.controller.verdictSeverity >= 2
            }

            PlasmaComponents3.Label {
                text: compactRoot.controller.cpuTempAvailable
                      ? Math.round(compactRoot.controller.cpuPackageTemp) + "°C " + compactRoot.velocityArrow(compactRoot.controller.thermalVelocity)
                      : "CPU temp N/A"
                font.bold: true
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize + 1
            }

            PlasmaComponents3.Label {
                visible: compactRoot.showPsiInPanel
                text: compactRoot.controller.psiAvailable ? Math.round(compactRoot.controller.responsivenessScore) + "% PSI" : "PSI N/A"
                opacity: 0.85
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            }

            PlasmaComponents3.Label {
                visible: compactRoot.showWattsInPanel
                text: compactRoot.controller.packagePowerAvailable
                      ? compactRoot.controller.packageWatts.toFixed(1) + "W" + (compactRoot.controller.packagePowerEstimated ? "~" : "")
                      : "W N/A"
                opacity: 0.85
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            }
        }
    }
}
