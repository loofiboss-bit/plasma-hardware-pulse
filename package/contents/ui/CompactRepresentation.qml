import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

Item {
    id: compactRoot

    required property var controller

    Layout.minimumWidth: pillRow.implicitWidth + Kirigami.Units.smallSpacing * 4
    Layout.preferredWidth: Layout.minimumWidth
    Layout.minimumHeight: Kirigami.Units.iconSizes.small

    function badgeColor(sev) {
        if (sev >= 3) return "#ef4444"
        if (sev === 2) return "#f97316"
        if (sev === 1) return "#f59e0b"
        return "#10b981"
    }

    function velocityArrow(vel) {
        if (vel >= 0.4) return "↑"
        if (vel >= 0.15) return "↗"
        if (vel <= -0.4) return "↓"
        if (vel <= -0.15) return "↘"
        return "•"
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: height / 2
        color: Qt.rgba(0.08, 0.11, 0.18, 0.55)
        border.width: 1
        border.color: compactRoot.badgeColor(compactRoot.controller.verdictSeverity)

        RowLayout {
            id: pillRow
            anchors.centerIn: parent
            spacing: Kirigami.Units.smallSpacing

            Rectangle {
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                radius: 4
                color: compactRoot.badgeColor(compactRoot.controller.verdictSeverity)
            }

            PlasmaComponents3.Label {
                text: Math.round(compactRoot.controller.cpuPackageTemp) + "°C "
                      + compactRoot.velocityArrow(compactRoot.controller.thermalVelocity)
                font.bold: true
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize + 1
            }

            PlasmaComponents3.Label {
                visible: Plasmoid.configuration.showPsiInPanel !== false
                text: "PSI " + Math.round(compactRoot.controller.responsivenessScore) + "%"
                opacity: 0.85
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            }

            PlasmaComponents3.Label {
                visible: Plasmoid.configuration.showWattsInPanel !== false
                text: compactRoot.controller.packageWatts.toFixed(1) + "W"
                opacity: 0.85
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.expanded = !root.expanded
    }
}
