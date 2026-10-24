pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: configRoot

    property string cfg_panelPreset: "custom"
    property alias cfg_pollIntervalMs: pollSpin.value
    property alias cfg_historyRetentionSeconds: retentionSpin.value
    property alias cfg_showCpuLoadInPanel: showCpuLoadCheck.checked
    property alias cfg_showGpuInPanel: showGpuCheck.checked
    property alias cfg_showRamInPanel: showRamCheck.checked
    property alias cfg_showIoInPanel: showIoCheck.checked
    property alias cfg_showWattsInPanel: showWattsCheck.checked
    property alias cfg_showPsiInPanel: showPsiCheck.checked
    property alias cfg_alwaysShowStatusInPanel: alwaysShowStatusCheck.checked
    property alias cfg_panelDisplayStyle: styleCombo.currentIndex
    property alias cfg_notificationsEnabled: notificationsCheck.checked

    function applyPreset(index) {
        if (index === 3) { cfg_panelPreset = "custom"; return }
        showCpuLoadCheck.checked = index === 2
        showGpuCheck.checked = index === 2
        showRamCheck.checked = index === 2
        showIoCheck.checked = index === 2
        showWattsCheck.checked = index !== 0
        showPsiCheck.checked = index !== 0
        alwaysShowStatusCheck.checked = index === 2
        cfg_panelPreset = ["minimal", "balanced", "detailed"][index]
    }

    function matchingPreset() {
        const optional = showCpuLoadCheck.checked || showGpuCheck.checked || showRamCheck.checked || showIoCheck.checked || alwaysShowStatusCheck.checked
        if (!optional && !showWattsCheck.checked && !showPsiCheck.checked) return 0
        if (!optional && showWattsCheck.checked && showPsiCheck.checked) return 1
        if (showCpuLoadCheck.checked && showGpuCheck.checked && showRamCheck.checked && showIoCheck.checked
            && showWattsCheck.checked && showPsiCheck.checked && alwaysShowStatusCheck.checked) return 2
        return 3
    }

    Kirigami.FormLayout {
        QQC2.SpinBox {
            id: pollSpin
            Kirigami.FormData.label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Sampling rate (ms):")
            from: 250
            to: 10000
            stepSize: 250
        }

        QQC2.Label {
            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "250–500 ms provides high-resolution spike capture; 1000 ms is recommended for balanced desktop use.")
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }

        QQC2.SpinBox {
            id: retentionSpin
            Kirigami.FormData.label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "History retention (seconds):")
            from: 300
            to: 1800
            stepSize: 300
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Panel representation")
        }

        QQC2.ComboBox {
            id: presetCombo
            Kirigami.FormData.label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Panel preset:")
            model: [KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Minimal"), KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Balanced"), KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Detailed"), KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Custom")]
            currentIndex: configRoot.matchingPreset()
            onActivated: index => configRoot.applyPreset(index)
        }

        QQC2.ComboBox {
            id: styleCombo
            Kirigami.FormData.label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Display style:")
            model: [
                KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Text Pill"),
                KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Mini Sparkline"),
                KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Ring Gauge")
            ]
        }

        QQC2.Label {
            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Minimal shows temperature and alerts. Balanced adds responsiveness and CPU package power. Detailed adds activity metrics. Long content is shortened; the tooltip keeps all current readings.")
            wrapMode: Text.WordWrap
        }

        QQC2.CheckBox {
            id: showCpuLoadCheck
            onToggled: configRoot.cfg_panelPreset = "custom"
            Kirigami.FormData.label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Visible metrics:")
            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Show CPU load (%) in panel")
        }

        QQC2.CheckBox {
            id: showGpuCheck
            onToggled: configRoot.cfg_panelPreset = "custom"
            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Show GPU load / temperature in panel")
        }

        QQC2.CheckBox {
            id: showRamCheck
            onToggled: configRoot.cfg_panelPreset = "custom"
            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Show RAM usage (%) in panel")
        }

        QQC2.CheckBox {
            id: showIoCheck
            onToggled: configRoot.cfg_panelPreset = "custom"
            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Show Disk / Network throughput (MiB/s) in panel")
        }

        QQC2.CheckBox {
            id: showWattsCheck
            onToggled: configRoot.cfg_panelPreset = "custom"
            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Show CPU package power draw (Watts) in panel")
        }

        QQC2.CheckBox {
            id: showPsiCheck
            onToggled: configRoot.cfg_panelPreset = "custom"
            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Show responsiveness index (0–100, higher is better) in panel")
        }

        QQC2.CheckBox {
            id: alwaysShowStatusCheck
            onToggled: configRoot.cfg_panelPreset = "custom"
            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Always show status headline in panel (otherwise shown only on active alerts)")
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Desktop Notifications")
        }

        QQC2.CheckBox {
            id: notificationsCheck
            Kirigami.FormData.label: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Alerts:")
            text: KI18n.i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Send KDE notification on active thermal throttling or critical PSI bottleneck")
        }
    }
}
