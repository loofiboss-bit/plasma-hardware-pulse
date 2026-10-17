import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: configRoot

    property alias cfg_pollIntervalMs: pollSpin.value
    property alias cfg_showWattsInPanel: showWattsCheck.checked
    property alias cfg_showPsiInPanel: showPsiCheck.checked
    property alias cfg_notificationsEnabled: notificationsCheck.checked

    Kirigami.FormLayout {
        QQC2.SpinBox {
            id: pollSpin
            Kirigami.FormData.label: i18n("Poll interval (ms):")
            from: 500
            to: 5000
            stepSize: 250
        }

        QQC2.CheckBox {
            id: showWattsCheck
            Kirigami.FormData.label: i18n("Panel Pill:")
            text: i18n("Show power draw (Watts) in panel")
        }

        QQC2.CheckBox {
            id: showPsiCheck
            text: i18n("Show calculated pressure index in panel")
        }

        QQC2.CheckBox {
            id: notificationsCheck
            Kirigami.FormData.label: i18n("Alerts:")
            text: i18n("Send KDE notification on active thermal throttling or critical PSI bottleneck")
        }
    }
}
