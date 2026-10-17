pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: configRoot

    property alias cfg_pollIntervalMs: pollSpin.value
    property alias cfg_historyRetentionSeconds: retentionSpin.value
    property alias cfg_showCpuLoadInPanel: showCpuLoadCheck.checked
    property alias cfg_showGpuInPanel: showGpuCheck.checked
    property alias cfg_showRamInPanel: showRamCheck.checked
    property alias cfg_showIoInPanel: showIoCheck.checked
    property alias cfg_showWattsInPanel: showWattsCheck.checked
    property alias cfg_showPsiInPanel: showPsiCheck.checked
    property alias cfg_alwaysShowStatusInPanel: alwaysShowStatusCheck.checked
    property alias cfg_notificationsEnabled: notificationsCheck.checked

    Kirigami.FormLayout {
        QQC2.SpinBox {
            id: pollSpin
            Kirigami.FormData.label: "Sampling rate (ms):"
            from: 500
            to: 5000
            stepSize: 250
        }

        QQC2.Label {
            text: "500 ms provides high-resolution spike capture; 1000 ms is recommended for balanced desktop use."
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }

        QQC2.SpinBox {
            id: retentionSpin
            Kirigami.FormData.label: "History retention (seconds):"
            from: 300
            to: 1800
            stepSize: 300
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Panel Pill Representation"
        }

        QQC2.CheckBox {
            id: showCpuLoadCheck
            Kirigami.FormData.label: "Visible metrics:"
            text: "Show CPU load (%) in panel"
        }

        QQC2.CheckBox {
            id: showGpuCheck
            text: "Show GPU load / temperature in panel"
        }

        QQC2.CheckBox {
            id: showRamCheck
            text: "Show RAM usage (%) in panel"
        }

        QQC2.CheckBox {
            id: showIoCheck
            text: "Show Disk / Network throughput (MiB/s) in panel"
        }

        QQC2.CheckBox {
            id: showWattsCheck
            text: "Show CPU package power draw (Watts) in panel"
        }

        QQC2.CheckBox {
            id: showPsiCheck
            text: "Show calculated pressure index (% PSI) in panel"
        }

        QQC2.CheckBox {
            id: alwaysShowStatusCheck
            text: "Always show status headline in panel (otherwise shown only on active alerts)"
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Desktop Notifications"
        }

        QQC2.CheckBox {
            id: notificationsCheck
            Kirigami.FormData.label: "Alerts:"
            text: "Send KDE notification on active thermal throttling or critical PSI bottleneck"
        }
    }
}
