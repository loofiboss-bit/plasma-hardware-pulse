import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

PlasmoidItem {
    id: root

    PulseTelemetryController {
        id: pulse
        pollIntervalMs: Plasmoid.configuration.pollIntervalMs || 1000
        notificationsEnabled: Plasmoid.configuration.notificationsEnabled !== undefined
                              ? Plasmoid.configuration.notificationsEnabled
                              : true
    }

    function severityColor(severity) {
        if (severity >= 3) return Kirigami.Theme.negativeTextColor
        if (severity === 2) return "#f97316"
        if (severity === 1) return Kirigami.Theme.neutralTextColor
        return Kirigami.Theme.positiveTextColor
    }

    toolTipMainText: pulse.verdictHeadline
    toolTipSubText: pulse.verdictCausalChain

    preferredRepresentation: compactRepresentation

    compactRepresentation: CompactRepresentation {
        controller: pulse
    }

    fullRepresentation: FullRepresentation {
        controller: pulse
    }
}
