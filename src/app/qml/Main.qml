pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import QtCore as QtCore
import com.github.loofi.hardwarepulse 1.0
import "MeasurementLabels.js" as MeasurementLabels

Kirigami.ApplicationWindow {
    id: appWindow
    objectName: "pulseWindow"
    title: KI18n.i18n("Loofi Pulse")
    width: 1160
    height: 780
    minimumWidth: 600
    minimumHeight: 480

    property string initialPageName: "overview"
    property string initialSectionName: ""
    property string currentPageName: "overview"

    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: appWindow.Kirigami.Theme.backgroundColor
        disabledColor: appWindow.Kirigami.Theme.disabledTextColor
        negativeColor: appWindow.Kirigami.Theme.negativeTextColor
        neutralColor: appWindow.Kirigami.Theme.neutralTextColor
        positiveColor: appWindow.Kirigami.Theme.positiveTextColor
        textColor: appWindow.Kirigami.Theme.textColor
        highlightColor: appWindow.Kirigami.Theme.highlightColor
    }

    QtCore.Settings {
        id: appSettings
        category: "LoofiPulse"
        property int savedPollInterval: 1000
        property int savedHistoryRetentionSeconds: 300
        property bool savedNotificationsEnabled: true
        property var savedNetworkInterfaces: []
        property bool savedExportIncludeProcessDetails: false
        property string savedTemperatureUnit: "C"
        property bool savedShowTrayIcon: true
        property bool savedMinimizeToTray: true
        property bool savedTrayCloseNoticeShown: false
    }

    property alias savedShowTrayIcon: appSettings.savedShowTrayIcon
    property alias savedMinimizeToTray: appSettings.savedMinimizeToTray
    property var tray: null

    onTrayChanged: {
        if (appWindow.tray) {
            appWindow.tray.visible = appSettings.savedShowTrayIcon
            appWindow.tray.minimizeToTrayOnClose = appSettings.savedMinimizeToTray
        }
    }

    PulseTelemetryController {
        id: telemetry
        objectName: "telemetryController"
        pollIntervalMs: appSettings.savedPollInterval
        historyRetentionSeconds: appSettings.savedHistoryRetentionSeconds
        notificationsEnabled: appSettings.savedNotificationsEnabled
        selectedNetworkInterfaces: appSettings.savedNetworkInterfaces
        exportIncludeProcessDetails: appSettings.savedExportIncludeProcessDetails
        temperatureUnit: appSettings.savedTemperatureUnit
        onSelectedNetworkInterfacesChanged: appSettings.savedNetworkInterfaces = selectedNetworkInterfaces
        onExportIncludeProcessDetailsChanged: appSettings.savedExportIncludeProcessDetails = exportIncludeProcessDetails
        onPollIntervalChanged: appSettings.savedPollInterval = pollIntervalMs
        onHistoryRetentionChanged: appSettings.savedHistoryRetentionSeconds = historyRetentionSeconds
        onNotificationsEnabledChanged: appSettings.savedNotificationsEnabled = notificationsEnabled
        onTemperatureUnitChanged: appSettings.savedTemperatureUnit = temperatureUnit
    }

    function updateCollectorActivity() {
        telemetry.setCollectorClientActive(appWindow.visible && appWindow.visibility !== Window.Minimized)
    }

    PulseSidebar {
        id: sidebar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        z: 20
        visible: appWindow.width >= 640
        theme: appWindow.theme
        controller: telemetry
        currentPage: appWindow.currentPageName
        onPageSelected: (pageId, section) => appWindow.switchToPage(pageId, section)
    }

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    Component.onCompleted: {
        updateCollectorActivity()
        if (appWindow.pageStack) {
            appWindow.pageStack.globalToolBar.style = Kirigami.ApplicationHeaderStyle.None
            appWindow.pageStack.anchors.leftMargin = Qt.binding(function() {
                return sidebar.visible ? sidebar.width : 0
            })
        }
    }
    onVisibleChanged: updateCollectorActivity()
    onVisibilityChanged: updateCollectorActivity()

    Shortcut {
        sequences: ["Ctrl+1", "Alt+1"]
        onActivated: appWindow.switchToPage("overview")
    }
    Shortcut {
        sequences: ["Ctrl+2", "Alt+2"]
        onActivated: appWindow.switchToPage("flight")
    }
    Shortcut {
        sequences: ["Ctrl+3", "Alt+3"]
        onActivated: appWindow.switchToPage("pressure")
    }
    Shortcut {
        sequences: ["Ctrl+4", "Alt+4"]
        onActivated: appWindow.switchToPage("power")
    }
    Shortcut {
        sequences: ["Ctrl+P"]
        onActivated: telemetry.recordingPaused = !telemetry.recordingPaused
    }
    Shortcut {
        sequence: "Ctrl+S"
        onActivated: telemetry.exportIncidentReportToFile()
    }
    Shortcut {
        sequence: "Ctrl+Shift+C"
        onActivated: telemetry.copyIncidentReportToClipboard()
    }

    Shortcut { sequence: "Ctrl+O"; onActivated: openSessionDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: saveSessionDialog.open() }
    Shortcut { sequence: "Ctrl+5"; onActivated: appWindow.switchToPage("processes") }
    Shortcut { sequence: "Ctrl+6"; onActivated: appWindow.switchToPage("storage") }
    Shortcut { sequences: ["Ctrl+,", "Ctrl+Alt+S"]; onActivated: appWindow.switchToPage("settings") }
    Shortcut { sequence: "Ctrl+Q"; onActivated: appWindow.requestQuit() }
    Shortcut { sequences: ["F1", "Ctrl+?"]; onActivated: shortcutsDialog.open() }

    FileDialog {
        id: openSessionDialog
        objectName: "openSessionDialog"
        title: KI18n.i18n("Open Pulse session")
        nameFilters: [KI18n.i18n("Pulse sessions (*.pulse-session)")]
        fileMode: FileDialog.OpenFile
        onAccepted: { telemetry.openSession(selectedFile.toString()); appWindow.switchToPage("flight") }
    }
    FileDialog {
        id: saveSessionDialog
        objectName: "saveSessionDialog"
        title: KI18n.i18n("Save Pulse session")
        nameFilters: [KI18n.i18n("Pulse sessions (*.pulse-session)")]
        defaultSuffix: "pulse-session"
        fileMode: FileDialog.SaveFile
        onAccepted: { appWindow.quitAfterSave = appWindow.saveForQuit; appWindow.saveForQuit = false; telemetry.saveSession(selectedFile.toString()) }
        onRejected: appWindow.cancelQuit()
    }
    property bool explicitQuit: false
    property bool saveForQuit: false
    property bool quitAfterSave: false
    property bool allowDiscard: false
    function requestQuit() {
        explicitQuit = true
        if (!telemetry.canQuitWithoutSaving()) {
            restoreFromBackground()
            unsavedDialog.open()
        } else appWindow.close()
    }
    function cancelQuit() {
        explicitQuit = false
        quitAfterSave = false
        saveForQuit = false
        allowDiscard = false
    }
    onClosing: close => {
        const toTray = !explicitQuit && appSettings.savedShowTrayIcon && appSettings.savedMinimizeToTray
            && appWindow.tray && appWindow.tray.available
        if (toTray) {
            close.accepted = false
            appWindow.hide()
            if (!appSettings.savedTrayCloseNoticeShown) {
                appSettings.savedTrayCloseNoticeShown = true
                appWindow.tray.showInformation(KI18n.i18n("Loofi Pulse"), KI18n.i18n("Loofi Pulse is still running in the system tray. Right-click the tray icon to quit."))
            }
        } else if (!allowDiscard && !telemetry.canQuitWithoutSaving()) {
            close.accepted = false
            explicitQuit = true
            unsavedDialog.open()
        } else {
            close.accepted = true
            Qt.quit()
        }
    }
    Connections {
        target: appWindow.tray
        function onQuitRequested() { appWindow.requestQuit() }
    }
    Connections {
        target: telemetry
        function onTelemetryUpdated() {
            if (appWindow.quitAfterSave && !telemetry.actionPending) {
                appWindow.quitAfterSave = false
                if (telemetry.lastActionSucceeded && telemetry.canQuitWithoutSaving()) appWindow.requestQuit()
                else appWindow.cancelQuit()
            }
        }
    }
    QQC2.Dialog {
        id: unsavedDialog
        objectName: "unsavedInvestigationDialog"
        title: KI18n.i18n("Save this investigation?")
        modal: true
        anchors.centerIn: parent
        width: Math.min(appWindow.width - 40, 480)
        standardButtons: QQC2.Dialog.Cancel
        onRejected: appWindow.cancelQuit()
        ColumnLayout {
            width: parent.width
            QQC2.Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: KI18n.i18n("This investigation has not been saved. Save a session to keep the original evidence before quitting.") }
            QQC2.Button { objectName: "saveBeforeQuitButton"; text: KI18n.i18n("Save session…"); enabled: !telemetry.actionPending; onClicked: { unsavedDialog.close(); appWindow.saveForQuit = true; saveSessionDialog.open() } }
            QQC2.Button { objectName: "discardAndQuitButton"; text: KI18n.i18n("Discard investigation and quit"); onClicked: { unsavedDialog.close(); appWindow.allowDiscard = true; appWindow.explicitQuit = true; appWindow.close() } }
        }
    }

    function openSession(path) { telemetry.openSession(path); switchToPage("flight") }
    function openIncident(id) { telemetry.selectIncident(id); switchToPage("flight", "inspector") }
    function navigateTo(name, section) { switchToPage(name, section) }
    function openAboutDialog() { aboutDialog.open() }
    function openShortcutsDialog() { shortcutsDialog.open() }
    function restoreFromBackground() {
        appWindow.show()
        appWindow.raise()
        appWindow.requestActivate()
    }

    function focusPageSection(item: var, section: string): void {
        if (item && typeof item.focusSection === "function") item.focusSection(section)
    }

    function switchToPage(name, section) {
        var targetComp = overviewComp
        var normalized = "overview"
        if (name === "flight") {
            targetComp = flightComp
            normalized = "flight"
        } else if (name === "pressure") {
            targetComp = pressureComp
            normalized = "pressure"
        } else if (name === "processes") {
            targetComp = processesComp
            normalized = "processes"
        } else if (name === "storage") {
            targetComp = storageComp
            normalized = "storage"
        } else if (name === "power") {
            targetComp = powerComp
            normalized = "power"
        } else if (name === "settings") {
            targetComp = settingsComp
            normalized = "settings"
        }
        appWindow.currentPageName = normalized
        var targetPage = appWindow.pageStack.depth > 0
            ? appWindow.pageStack.replace(targetComp) : appWindow.pageStack.push(targetComp)
        Qt.callLater(function() { appWindow.focusPageSection(targetPage, section || "") })
    }

    function pageTitle(pageName) {
        if (pageName === "flight") return KI18n.i18n("History")
        if (pageName === "pressure") return KI18n.i18n("Memory & Pressure")
        if (pageName === "power") return KI18n.i18n("Power & Cooling")
        if (pageName === "processes") return KI18n.i18n("Processes")
        if (pageName === "storage") return KI18n.i18n("Storage & Network")
        if (pageName === "settings") return KI18n.i18n("Settings")
        return KI18n.i18n("Overview")
    }

    function applyInitialNavigation() { switchToPage(initialPageName, initialSectionName) }
    onInitialPageNameChanged: Qt.callLater(applyInitialNavigation)
    onInitialSectionNameChanged: Qt.callLater(applyInitialNavigation)

    QQC2.Dialog {
        id: settingsDialog
        objectName: "settingsDialog"
        title: KI18n.i18n("Settings")
        modal: true
        anchors.centerIn: parent
        width: Math.min(appWindow.width - 40, 560)
        height: Math.min(appWindow.height - 60, settingsContent.implicitHeight + 100)
        standardButtons: QQC2.Dialog.Close
        QQC2.ScrollView {
            anchors.fill: parent
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                id: settingsContent
                width: parent.width
                spacing: Kirigami.Units.mediumSpacing
                QQC2.Label { text: KI18n.i18n("Sampling interval (ms)"); font.bold: true }
                QQC2.SpinBox {
                    objectName: "samplingIntervalSetting"
                    from: 250; to: 10000; stepSize: 250
                    value: telemetry.pollIntervalMs
                    onValueModified: telemetry.pollIntervalMs = value
                    Accessible.name: KI18n.i18n("Sampling interval in milliseconds")
                }
                QQC2.Label { text: KI18n.i18n("History retention"); font.bold: true }
                QQC2.ComboBox {
                    objectName: "historyRetentionSetting"
                    model: [KI18n.i18n("5 minutes"), KI18n.i18n("15 minutes"), KI18n.i18n("30 minutes")]
                    currentIndex: telemetry.historyRetentionSeconds === 1800 ? 2 : telemetry.historyRetentionSeconds === 900 ? 1 : 0
                    onActivated: telemetry.historyRetentionSeconds = [300, 900, 1800][currentIndex]
                    Accessible.name: KI18n.i18n("History retention")
                }
                QQC2.CheckBox {
                    text: KI18n.i18n("Enable incident notifications")
                    checked: telemetry.notificationsEnabled
                    onToggled: telemetry.notificationsEnabled = checked
                }
                QQC2.CheckBox {
                    text: KI18n.i18n("Include process names and IDs in reports")
                    checked: telemetry.exportIncludeProcessDetails
                    onToggled: telemetry.exportIncludeProcessDetails = checked
                }
                QQC2.CheckBox {
                    text: KI18n.i18n("Continue monitoring when all windows close")
                    checked: telemetry.backgroundMonitoring
                    onToggled: telemetry.backgroundMonitoring = checked
                }
                QQC2.CheckBox {
                    text: KI18n.i18n("Start monitoring at login")
                    checked: telemetry.startAtLogin
                    onToggled: telemetry.startAtLogin = checked
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: KI18n.i18n("Background monitoring and login startup are separate opt-ins. Recordings stay in memory until you save a session. The process-detail privacy setting also applies to sessions.")
                }
                QQC2.Label { text: KI18n.i18n("Network interfaces"); font.bold: true }
                QQC2.CheckBox {
                    text: KI18n.i18n("Automatically use physical interfaces")
                    checked: telemetry.selectedNetworkInterfaces.length === 0
                    onToggled: {
                        if (checked) telemetry.selectedNetworkInterfaces = []
                        else {
                            var ids = []
                            for (var i = 0; i < telemetry.networkInterfaces.length; ++i) {
                                var iface = telemetry.networkInterfaces[i]
                                if (iface.physical && iface.available) ids.push(iface.id)
                            }
                            if (ids.length > 0) telemetry.selectedNetworkInterfaces = ids
                        }
                    }
                }
                Repeater {
                    model: telemetry.networkInterfaces
                    delegate: QQC2.CheckBox {
                        required property var modelData
                        text: modelData.name + (modelData.physical ? KI18n.i18n(" (physical)") : KI18n.i18n(" (virtual)"))
                              + (modelData.available ? "" : KI18n.i18n(" · unavailable"))
                        checked: modelData.included
                        onToggled: {
                            var ids = telemetry.selectedNetworkInterfaces.slice()
                            if (ids.length === 0) {
                                for (var i = 0; i < telemetry.networkInterfaces.length; ++i) {
                                    var candidate = telemetry.networkInterfaces[i]
                                    if (candidate.included) ids.push(candidate.id)
                                }
                            }
                            var index = ids.indexOf(modelData.id)
                            if (checked && index < 0) ids.push(modelData.id)
                            else if (!checked && index >= 0) ids.splice(index, 1)
                            telemetry.selectedNetworkInterfaces = ids
                        }
                    }
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: KI18n.i18n("An empty selection uses physical interfaces automatically. Reports include process details only when explicitly enabled.")
                }
            }
        }
    }

    QQC2.Dialog {
        id: aboutDialog
        objectName: "aboutDialog"
        title: KI18n.i18n("About Loofi Pulse")
        modal: true
        anchors.centerIn: parent
        width: Math.min(appWindow.width - 40, 520)
        standardButtons: QQC2.Dialog.Close

        ColumnLayout {
            width: parent.width
            spacing: Kirigami.Units.largeSpacing

            RowLayout {
                spacing: Kirigami.Units.largeSpacing
                Kirigami.Icon {
                    source: "utilities-system-monitor"
                    implicitWidth: 64
                    implicitHeight: 64
                }
                ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing
                    QQC2.Label {
                        text: KI18n.i18n("Loofi Pulse")
                        font.bold: true
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.4
                    }
                    QQC2.Label {
                        text: KI18n.i18n("Version ") + Qt.application.version + KI18n.i18n(" · KDE Plasma 6")
                        color: Kirigami.Theme.disabledTextColor
                    }
                    QQC2.Label {
                        text: KI18n.i18n("Local hardware monitoring and incident investigation for KDE Plasma.")
                        wrapMode: Text.WordWrap
                        Layout.maximumWidth: 380
                    }
                }
            }

            Kirigami.Separator { Layout.fillWidth: true }

            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                QQC2.Label {
                    text: KI18n.i18n("License: GNU General Public License v3.0")
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    text: KI18n.i18n("Author: Loofi Boss")
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    text: KI18n.i18n("GitHub: https://github.com/loofiboss-bit/plasma-hardware-pulse")
                    color: Kirigami.Theme.highlightColor
                }
            }
        }
    }

    QQC2.Dialog {
        id: shortcutsDialog
        objectName: "shortcutsDialog"
        title: KI18n.i18n("Keyboard Shortcuts")
        modal: true
        anchors.centerIn: parent
        width: Math.min(appWindow.width - 40, 540)
        standardButtons: QQC2.Dialog.Close

        ColumnLayout {
            width: parent.width
            spacing: Kirigami.Units.mediumSpacing

            QQC2.Label {
                text: KI18n.i18n("Navigation & Actions")
                font.bold: true
            }

            GridLayout {
                columns: 2
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.smallSpacing

                QQC2.Label { text: "Ctrl+1 / Alt+1"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("Overview page") }

                QQC2.Label { text: "Ctrl+2 / Alt+2"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("History & Flight Recorder") }

                QQC2.Label { text: "Ctrl+3 / Alt+3"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("Memory & PSI Pressure") }

                QQC2.Label { text: "Ctrl+4 / Alt+4"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("Power & Cooling") }

                QQC2.Label { text: "Ctrl+5"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("Processes") }

                QQC2.Label { text: "Ctrl+6"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("Storage & Network") }

                QQC2.Label { text: "Ctrl+,"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("Settings page") }

                QQC2.Label { text: "Ctrl+P"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("Pause / Resume Flight Recorder") }

                QQC2.Label { text: "Ctrl+S"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("Export incident report") }

                QQC2.Label { text: "Ctrl+Shift+C"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("Copy report to clipboard") }

                QQC2.Label { text: "Ctrl+O / Ctrl+Shift+S"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("Open / Save session file") }

                QQC2.Label { text: "F1 / Ctrl+?"; font.family: "monospace"; font.bold: true }
                QQC2.Label { text: KI18n.i18n("Show this cheat sheet") }
            }
        }
    }

    globalDrawer: Kirigami.GlobalDrawer {
        id: globalDrawer
        isMenu: false
        handleVisible: false
        actions: [
            Kirigami.Action {
                objectName: "navigateOverview"
                text: KI18n.i18n("Overview")
                icon.name: "view-statistics"
                checked: appWindow.currentPageName === "overview"
                onTriggered: appWindow.switchToPage("overview")
            },
            Kirigami.Action {
                objectName: "navigatePower"
                text: KI18n.i18n("Power & Cooling")
                icon.name: "battery-profile-performance"
                checked: appWindow.currentPageName === "power"
                onTriggered: appWindow.switchToPage("power")
            },
            Kirigami.Action {
                objectName: "navigatePressure"
                text: KI18n.i18n("Memory & Pressure")
                icon.name: "memory"
                checked: appWindow.currentPageName === "pressure"
                onTriggered: appWindow.switchToPage("pressure")
            },
            Kirigami.Action {
                objectName: "navigateStorage"
                text: KI18n.i18n("Storage & Network")
                icon.name: "drive-harddisk"
                checked: appWindow.currentPageName === "storage"
                onTriggered: appWindow.switchToPage("storage")
            },
            Kirigami.Action {
                objectName: "navigateProcesses"
                text: KI18n.i18n("Processes")
                icon.name: "view-process-all"
                checked: appWindow.currentPageName === "processes"
                onTriggered: appWindow.switchToPage("processes")
            },
            Kirigami.Action {
                objectName: "navigateHistory"
                text: KI18n.i18n("History")
                icon.name: "chronometer"
                checked: appWindow.currentPageName === "flight"
                onTriggered: appWindow.switchToPage("flight")
            },
            Kirigami.Action {
                separator: true
            },
            Kirigami.Action {
                objectName: "navigateSettings"
                text: KI18n.i18n("Settings")
                icon.name: "configure"
                checked: appWindow.currentPageName === "settings"
                onTriggered: appWindow.switchToPage("settings")
            },
            Kirigami.Action {
                text: KI18n.i18n("Open session…")
                icon.name: "document-open"
                onTriggered: openSessionDialog.open()
            },
            Kirigami.Action {
                text: KI18n.i18n("Save session…")
                icon.name: "document-save-as"
                onTriggered: saveSessionDialog.open()
            },
            Kirigami.Action {
                text: KI18n.i18n("Snooze notifications for 15 minutes")
                icon.name: "notifications-disabled"
                onTriggered: telemetry.snoozeNotifications()
            },
            Kirigami.Action {
                separator: true
            },
            Kirigami.Action {
                text: telemetry.recordingPaused ? KI18n.i18n("Resume Flight Recorder") : KI18n.i18n("Pause Flight Recorder")
                icon.name: telemetry.recordingPaused ? "media-playback-start" : "media-playback-pause"
                onTriggered: telemetry.recordingPaused = !telemetry.recordingPaused
            },
            Kirigami.Action {
                objectName: "openAbout"
                text: KI18n.i18n("About Loofi Pulse")
                icon.name: "help-about"
                onTriggered: appWindow.openAboutDialog()
            },
            Kirigami.Action {
                objectName: "openShortcuts"
                text: KI18n.i18n("Keyboard Shortcuts")
                icon.name: "input-keyboard"
                onTriggered: appWindow.openShortcutsDialog()
            },
            Kirigami.Action {
                text: KI18n.i18n("Quit Loofi Pulse")
                icon.name: "application-exit"
                onTriggered: appWindow.requestQuit()
            },
            Kirigami.Action {
                objectName: "openSettings"
                text: KI18n.i18n("Quick Settings…")
                icon.name: "settings-configure"
                visible: false
                onTriggered: settingsDialog.open()
            }
        ]
    }

    header: Rectangle {
        width: appWindow.width
        implicitHeight: 38
        color: "#0e131b"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: "#1a222e"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 10

            Rectangle {
                width: 22
                height: 22
                radius: 5
                color: "#2563eb"
                QQC2.Label {
                    anchors.centerIn: parent
                    text: "Lp"
                    font.bold: true
                    font.pointSize: 9
                    color: "#ffffff"
                }
            }

            QQC2.Label {
                text: "Loofi Pulse"
                font.bold: true
                font.pointSize: 11
                color: "#ffffff"
            }

            QQC2.Label {
                text: "|"
                font.pointSize: 11
                color: "#334155"
            }

            QQC2.Label {
                text: KI18n.i18n("Systemövervakning & prestandastyrning")
                font.pointSize: 9.5
                color: "#94a3b8"
            }

            Item { Layout.fillWidth: true }

            // pageSelector for tests and small screens
            QQC2.ComboBox {
                id: pageSelector
                objectName: "pageSelector"
                visible: appWindow.width < 900
                implicitHeight: 28
                Accessible.name: KI18n.i18n("Navigate to page")
                textRole: "label"
                valueRole: "id"
                model: [
                    { id: "overview", label: KI18n.i18n("Overview") },
                    { id: "flight", label: KI18n.i18n("History") },
                    { id: "pressure", label: KI18n.i18n("Memory & Pressure") },
                    { id: "power", label: KI18n.i18n("Power & Cooling") },
                    { id: "processes", label: KI18n.i18n("Processes") },
                    { id: "storage", label: KI18n.i18n("Storage & Network") },
                    { id: "settings", label: KI18n.i18n("Settings") }
                ]
                currentIndex: {
                    for (let i = 0; i < model.length; ++i)
                        if (model[i].id === appWindow.currentPageName) return i
                    return 0
                }
                onActivated: index => appWindow.switchToPage(model[index].id)
            }
        }
    }

    footer: Rectangle {
        width: appWindow.width
        implicitHeight: 28
        color: appWindow.theme.surfaceColor

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: appWindow.theme.cardBorderColor
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 12

            // Left: status indicator
            RowLayout {
                spacing: 6
                Rectangle {
                    width: 7
                    height: 7
                    radius: 3.5
                    color: appWindow.theme.severityColor(telemetry.verdictSeverity)
                }

                QQC2.Label {
                    text: telemetry.recordingPaused
                          ? KI18n.i18nd("loofi-pulse", "Paused: Flight Recorder")
                          : (telemetry.lastActionStatus.length > 0 ? telemetry.lastActionStatus : KI18n.i18nd("loofi-pulse", "Ready: Flight Recorder running"))
                    font.pointSize: appWindow.theme.fontCaption
                    color: appWindow.theme.mutedTextColor
                }
            }

            Item { Layout.fillWidth: true }

            // Right: view mode | collector interval | clock
            RowLayout {
                spacing: 10

                RowLayout {
                    spacing: 4
                    Kirigami.Icon { source: "view-list"; implicitWidth: 12; implicitHeight: 12; color: appWindow.theme.faintTextColor }
                    QQC2.Label {
                        text: (telemetry.sessionMode ? KI18n.i18nd("loofi-pulse", "View: Saved session") : KI18n.i18nd("loofi-pulse", "View: Live telemetry"))
                        font.pointSize: appWindow.theme.fontCaption
                        color: appWindow.theme.mutedTextColor
                    }
                }

                QQC2.Label { text: "·"; color: appWindow.theme.faintTextColor }

                QQC2.Label {
                    text: KI18n.i18nd("loofi-pulse", "Collector: ") + telemetry.collectorState + " · " + telemetry.pollIntervalMs + " ms"
                    font.pointSize: appWindow.theme.fontCaption
                    color: appWindow.theme.mutedTextColor
                }

                QQC2.Label { text: "·"; color: appWindow.theme.faintTextColor }

                RowLayout {
                    spacing: 4
                    Kirigami.Icon { source: "appointment-recurring"; implicitWidth: 12; implicitHeight: 12; color: appWindow.theme.faintTextColor }
                    QQC2.Label {
                        text: Qt.formatDateTime(new Date(), "hh:mm · yyyy-MM-dd")
                        font.pointSize: appWindow.theme.fontCaption
                        color: appWindow.theme.mutedTextColor
                    }
                }
            }
        }
    }

    Component {
        id: overviewComp
        OverviewPage {
            controller: telemetry
            onNavigateRequested: (pageName, section) => appWindow.switchToPage(pageName, section)
        }
    }

    Component {
        id: flightComp
        FlightRecorderPage {
            controller: telemetry
            onSaveSessionRequested: saveSessionDialog.open()
            onOpenSessionRequested: openSessionDialog.open()
            onOpenSettingsRequested: appWindow.switchToPage("settings")
        }
    }

    Component {
        id: pressureComp
        PressureMemoryPage {
            controller: telemetry
        }
    }

    Component {
        id: powerComp
        PowerThermalPage {
            controller: telemetry
        }
    }

    Component { id: processesComp; ProcessesPage { controller: telemetry } }
    Component { id: storageComp; StorageNetworkPage { controller: telemetry } }
    Component { id: settingsComp; SettingsPage { controller: telemetry; appWindow: appWindow } }

    pageStack.initialPage: overviewComp
}
