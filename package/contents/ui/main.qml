pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import com.github.loofi.hardwarepulse 1.0

PlasmoidItem {
    id: root

    PulseTelemetryController {
        id: pulse
        pollIntervalMs: Plasmoid.configuration.pollIntervalMs || 1000
        historyRetentionSeconds: Plasmoid.configuration.historyRetentionSeconds || 300
        notificationsEnabled: Plasmoid.configuration.notificationsEnabled !== undefined
                              ? Plasmoid.configuration.notificationsEnabled
                              : true
    }

    toolTipMainText: pulse.verdictHeadline
    toolTipSubText: {
        let lines = [pulse.verdictCausalChain]
        lines.push("CPU load: " + (pulse.cpuLoadAvailable ? Math.round(pulse.cpuLoadPercent) + "%" : "Unavailable"))
        lines.push("CPU temperature: " + (pulse.cpuTempAvailable ? Math.round(pulse.cpuPackageTemp) + "°C" : "Unavailable"))
        lines.push("GPU activity: " + (pulse.gpuBusyAvailable ? Math.round(pulse.gpuBusyPercent) + "%" : "Unavailable"))
        lines.push("Memory use: " + (pulse.memoryAvailable ? Math.round(pulse.ramUsedPercent) + "%" : "Unavailable"))
        lines.push("Responsiveness index: " + (pulse.psiAvailable ? Math.round(pulse.responsivenessScore) + "/100 (higher is better)" : "Unavailable"))
        lines.push("CPU package power: " + (pulse.packagePowerAvailable ? pulse.packageWatts.toFixed(1) + " W" + (pulse.packagePowerEstimated ? " (estimated)" : "") : "Unavailable"))
        lines.push("Battery discharge: " + (pulse.batteryPowerAvailable && pulse.batteryStatus === "Discharging" ? pulse.batteryWatts.toFixed(1) + " W" : "Unavailable or not discharging"))
        lines.push("Disk: " + (pulse.diskThroughputAvailable ? (pulse.diskReadMibPerSec + pulse.diskWriteMibPerSec).toFixed(1) + " MiB/s" : "Unavailable"))
        lines.push("Network: " + (pulse.netThroughputAvailable ? (pulse.netRxMibPerSec + pulse.netTxMibPerSec).toFixed(1) + " MiB/s" : "Unavailable"))
        return lines.join("\n")
    }

    preferredRepresentation: compactRepresentation

    compactRepresentation: CompactRepresentation {
        controller: pulse
        expanded: root.expanded
        verticalPanel: Plasmoid.formFactor === PlasmaCore.Types.Vertical
        showCpuLoadInPanel: Plasmoid.configuration.showCpuLoadInPanel === true
        showGpuInPanel: Plasmoid.configuration.showGpuInPanel === true
        showRamInPanel: Plasmoid.configuration.showRamInPanel === true
        showIoInPanel: Plasmoid.configuration.showIoInPanel === true
        showPsiInPanel: Plasmoid.configuration.showPsiInPanel !== false
        showWattsInPanel: Plasmoid.configuration.showWattsInPanel !== false
        alwaysShowStatusInPanel: Plasmoid.configuration.alwaysShowStatusInPanel === true
        onActivationRequested: root.expanded = !root.expanded
    }

    fullRepresentation: FullRepresentation {
        controller: pulse
    }
}
