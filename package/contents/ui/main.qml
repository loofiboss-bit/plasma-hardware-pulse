pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
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

    toolTipMainText: pulse.verdictHeadline
    toolTipSubText: pulse.verdictCausalChain

    preferredRepresentation: compactRepresentation

    compactRepresentation: CompactRepresentation {
        controller: pulse
        expanded: root.expanded
        showPsiInPanel: Plasmoid.configuration.showPsiInPanel !== false
        showWattsInPanel: Plasmoid.configuration.showWattsInPanel !== false
        alwaysShowStatusInPanel: Plasmoid.configuration.alwaysShowStatusInPanel === true
        onActivationRequested: root.expanded = !root.expanded
    }

    fullRepresentation: FullRepresentation {
        controller: pulse
    }
}
