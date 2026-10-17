pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: configRoot

    property alias cfg_pollIntervalMs: pollSpin.value
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

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Panel Pill Representation"
        }

        QQC2.CheckBox {
            id: showWattsCheck
            Kirigami.FormData.label: "Visible metrics:"
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
