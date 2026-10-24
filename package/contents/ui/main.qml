pragma ComponentBehavior: Bound

import QtQuick
import org.kde.ki18n
import "TemperatureLabels.js" as TemperatureLabels
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

    toolTipMainText: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", pulse.verdictHeadline)
    toolTipSubText: {
        let lines = [pulse.verdictCausalChain]
        lines.push(KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU load: ") + (pulse.cpuLoadAvailable ? Math.round(pulse.cpuLoadPercent) + "%" : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Unavailable")))
        lines.push(KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU temperature: ") + (pulse.cpuTempAvailable ? TemperatureLabels.formatTemperature(pulse.cpuPackageTemp, pulse.temperatureUnit) : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Unavailable")))
        lines.push(KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "GPU activity: ") + (pulse.gpuBusyAvailable ? Math.round(pulse.gpuBusyPercent) + "%" : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Unavailable")))
        lines.push(KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Memory use: ") + (pulse.memoryAvailable ? Math.round(pulse.ramUsedPercent) + "%" : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Unavailable")))
        lines.push(KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Responsiveness index: ") + (pulse.psiAvailable ? Math.round(pulse.responsivenessScore) + "/100 (higher is better)" : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Unavailable")))
        lines.push(KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "CPU package power: ") + (pulse.packagePowerAvailable ? pulse.packageWatts.toFixed(1) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " W") + (pulse.packagePowerEstimated ? KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " (estimated)") : "") : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Unavailable")))
        lines.push(KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Battery discharge: ") + (pulse.batteryPowerAvailable && pulse.batteryStatus === "Discharging" ? pulse.batteryWatts.toFixed(1) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " W") : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Unavailable or not discharging")))
        lines.push(KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Disk: ") + (pulse.diskThroughputAvailable ? (pulse.diskReadMibPerSec + pulse.diskWriteMibPerSec).toFixed(1) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " MiB/s") : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Unavailable")))
        lines.push(KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Network: ") + (pulse.netThroughputAvailable ? (pulse.netRxMibPerSec + pulse.netTxMibPerSec).toFixed(1) + KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", " MiB/s") : KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Unavailable")))
        return lines.join("\n")
    }

    preferredRepresentation: compactRepresentation

    compactRepresentation: CompactRepresentation {
        controller: pulse
        expanded: root.expanded
        verticalPanel: Plasmoid.formFactor === PlasmaCore.Types.Vertical
        panelDisplayStyle: Plasmoid.configuration.panelDisplayStyle || 0
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
