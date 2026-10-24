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
        title: KI18n.i18n("Loofi Pulse")
        titleIcon: "utilities-system-monitor"
        isMenu: false

        actions: [
            Kirigami.Action {
                objectName: "navigateOverview"
                text: KI18n.i18n("Overview")
                icon.name: "view-statistics"
                checked: appWindow.currentPageName === "overview"
                onTriggered: appWindow.switchToPage("overview")
            },
            Kirigami.Action {
                objectName: "navigateHistory"
                text: KI18n.i18n("History")
                icon.name: "chronometer"
                checked: appWindow.currentPageName === "flight"
                onTriggered: appWindow.switchToPage("flight")
            },
            Kirigami.Action {
                objectName: "navigatePressure"
                text: KI18n.i18n("Memory & Pressure")
                icon.name: "memory"
                checked: appWindow.currentPageName === "pressure"
                onTriggered: appWindow.switchToPage("pressure")
            },
            Kirigami.Action {
                objectName: "navigatePower"
                text: KI18n.i18n("Power & Cooling")
                icon.name: "battery-profile-performance"
                checked: appWindow.currentPageName === "power"
                onTriggered: appWindow.switchToPage("power")
            },
            Kirigami.Action {
                objectName: "navigateProcesses"
                text: KI18n.i18n("Processes")
                icon.name: "view-process-all"
                checked: appWindow.currentPageName === "processes"
                onTriggered: appWindow.switchToPage("processes")
            },
            Kirigami.Action {
                objectName: "navigateStorage"
                text: KI18n.i18n("Storage & Network")
                icon.name: "drive-harddisk"
                checked: appWindow.currentPageName === "storage"
                onTriggered: appWindow.switchToPage("storage")
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

    menuBar: Rectangle {
        width: appWindow.width
        implicitHeight: topBarCol.implicitHeight + Kirigami.Units.smallSpacing * 2
        color: theme.surfaceColor
        border.width: 0

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: theme.cardBorderColor
        }

        ColumnLayout {
            id: topBarCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Kirigami.Units.smallSpacing
            anchors.leftMargin: Kirigami.Units.largeSpacing
            anchors.rightMargin: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing

            Kirigami.InlineMessage {
                id: collectorAlertMessage
                Layout.fillWidth: true
                visible: !telemetry.collectorConnected
                type: Kirigami.MessageType.Error
                text: KI18n.i18n("Collector service is disconnected (") + telemetry.collectorState + KI18n.i18n("). Real-time telemetry is paused.")
                actions: [
                    Kirigami.Action {
                        text: KI18n.i18n("Reconnect")
                        icon.name: "view-refresh"
                        onTriggered: telemetry.reconnectCollector()
                    }
                ]
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                // Live System Verdict Status Pill
                Rectangle {
                    radius: theme.pillRadius
                    color: theme.severityBgColor(telemetry.verdictSeverity)
                    border.width: 1
                    border.color: theme.severityBorderColor(telemetry.verdictSeverity)
                    implicitWidth: Math.min(appWindow.width * 0.42, statusPillRow.implicitWidth + 16)
                    implicitHeight: statusPillRow.implicitHeight + 8

                    RowLayout {
                        id: statusPillRow
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        Rectangle {
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: theme.severityColor(telemetry.verdictSeverity)
                        }

                        QQC2.Label {
                            text: theme.severityBadgeText(telemetry.verdictSeverity)
                            color: theme.severityColor(telemetry.verdictSeverity)
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }

                        QQC2.Label {
                            visible: appWindow.width >= 820
                            text: KI18n.i18n("· ") + KI18n.i18n(telemetry.verdictHeadline)
                            color: Kirigami.Theme.textColor
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    QQC2.AbstractButton {
                        objectName: "statusExplanationButton"
                        anchors.fill: parent
                        Accessible.name: KI18n.i18n(telemetry.verdictHeadline) + KI18n.i18n(". Open status explanation")
                        onClicked: appWindow.switchToPage("overview", "status")
                        background: Rectangle { color: "transparent"; border.width: parent.activeFocus ? 2 : 0; border.color: Kirigami.Theme.focusColor }
                    }
                }

                // Interactive Power Profile Quick-Switcher Pill
                Rectangle {
                    visible: telemetry.powerProfileServiceAvailable && telemetry.availablePowerProfiles.length > 0
                    radius: theme.pillRadius
                    color: theme.subtleSurfaceColor
                    border.width: 1
                    border.color: theme.subtleBorderColor
                    implicitHeight: powerProfileRow.implicitHeight + 6
                    implicitWidth: powerProfileRow.implicitWidth + 12

                    RowLayout {
                        id: powerProfileRow
                        anchors.centerIn: parent
                        spacing: 2

                        Kirigami.Icon {
                            source: telemetry.activePowerProfile === "power-saver" ? "battery-profile-powersave"
                                  : (telemetry.activePowerProfile === "performance" ? "battery-profile-performance" : "battery-profile-balanced")
                            Layout.preferredWidth: 14
                            Layout.preferredHeight: 14
                        }

                        Repeater {
                            model: telemetry.availablePowerProfiles
                            delegate: QQC2.Button {
                                id: profQuickBtn
                                required property string modelData
                                text: modelData === "power-saver" ? KI18n.i18n("Saver")
                                    : (modelData === "performance" ? KI18n.i18n("Perf") : KI18n.i18n("Bal"))
                                flat: true
                                checkable: true
                                checked: telemetry.activePowerProfile === profQuickBtn.modelData
                                enabled: !telemetry.powerProfileChangePending
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                font.bold: checked
                                padding: 4
                                Accessible.name: KI18n.i18n("Power profile ") + profQuickBtn.modelData
                                onClicked: telemetry.setPowerProfile(profQuickBtn.modelData)
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // Quick telemetry readout pills on wide screens
                Rectangle {
                    visible: appWindow.width >= 1000
                    radius: theme.pillRadius
                    color: theme.subtleSurfaceColor
                    border.width: 1
                    border.color: theme.subtleBorderColor
                    implicitWidth: quickMetricsRow.implicitWidth + 16
                    implicitHeight: quickMetricsRow.implicitHeight + 6

                    RowLayout {
                        id: quickMetricsRow
                        anchors.centerIn: parent
                        spacing: 8

                        QQC2.Label {
                            text: telemetry.cpuTempAvailable
                                  ? MeasurementLabels.formatTemperature(telemetry.cpuPackageTemp, telemetry.temperatureUnit)
                                  : KI18n.i18n("Temp N/A")
                            color: theme.tempAccent
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }

                        QQC2.Label {
                            text: KI18n.i18n("·")
                            color: Kirigami.Theme.disabledTextColor
                        }

                        QQC2.Label {
                            text: telemetry.cpuLoadAvailable
                                  ? Math.round(telemetry.cpuLoadPercent) + KI18n.i18n("% CPU")
                                  : KI18n.i18n("CPU N/A")
                            color: theme.cpuAccent
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }

                        QQC2.Label {
                            text: KI18n.i18n("·")
                            color: Kirigami.Theme.disabledTextColor
                        }

                        QQC2.Label {
                            text: telemetry.psiAvailable
                                  ? KI18n.i18n("Responsiveness ") + Math.round(telemetry.responsivenessScore) + "/100"
                                  : KI18n.i18n("Index unavailable")
                            color: theme.psiAccent
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }

                        QQC2.Label {
                            visible: telemetry.packagePowerAvailable
                            text: KI18n.i18n("· CPU package ") + telemetry.packageWatts.toFixed(1) + KI18n.i18n(" W")
                                  + (telemetry.packagePowerEstimated ? KI18n.i18n(" (est.)") : "")
                            color: theme.powerAccent
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }
                }

                // Live / Pause pill button
                PulseButton {
                    icon.name: telemetry.recordingPaused ? "media-playback-start" : "media-playback-pause"
                    text: telemetry.recordingPaused ? KI18n.i18n("Recording paused") : (telemetry.collectorState + " · " + telemetry.pollIntervalMs + KI18n.i18n(" ms"))
                    display: appWindow.width < 680 ? QQC2.AbstractButton.IconOnly : QQC2.AbstractButton.TextBesideIcon
                    Accessible.name: telemetry.recordingPaused ? KI18n.i18n("Resume Flight Recorder") : KI18n.i18n("Pause Flight Recorder")
                    onClicked: telemetry.recordingPaused = !telemetry.recordingPaused
                }
            }

            // Collector and Mode Status Indicator
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    radius: theme.pillRadius
                    color: telemetry.sessionMode ? theme.accentTint(theme.neutralColor, 0.16) : theme.subtleSurfaceColor
                    border.width: 1
                    border.color: telemetry.sessionMode ? theme.neutralColor : theme.subtleBorderColor
                    implicitWidth: collStatusRow.implicitWidth + 12
                    implicitHeight: collStatusRow.implicitHeight + 4

                    RowLayout {
                        id: collStatusRow
                        anchors.centerIn: parent
                        spacing: 6
                        Rectangle {
                            Layout.preferredWidth: 6
                            Layout.preferredHeight: 6
                            radius: 3
                            color: telemetry.collectorState.indexOf("Disconnected") === 0
                                   ? Kirigami.Theme.negativeTextColor
                                   : (telemetry.sessionMode ? theme.neutralColor : Kirigami.Theme.positiveTextColor)
                        }
                        QQC2.Label {
                            text: KI18n.i18n("Collector: ") + telemetry.collectorState
                                  + (telemetry.sessionMode ? KI18n.i18n(" · HISTORICAL investigation (live monitoring continues separately)") : KI18n.i18n(" · Live recording"))
                                  + (telemetry.unsavedInvestigation ? KI18n.i18n(" · Unsaved") : "")
                            color: telemetry.sessionMode ? theme.neutralColor : Kirigami.Theme.disabledTextColor
                            font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                            font.bold: telemetry.sessionMode
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // Segmented View Switcher Row (Recessed track container with elevated active tab)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: appWindow.width < 900 ? pageSelector.implicitHeight + 8 : navTabsRow.implicitHeight + 8
                radius: theme.innerRadius + 2
                color: theme.cardNestedBg
                border.width: 1
                border.color: theme.subtleBorderColor

                QQC2.ComboBox {
                    id: pageSelector
                    objectName: "pageSelector"
                    visible: appWindow.width < 900
                    anchors.fill: parent
                    anchors.margins: 4
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

                RowLayout {
                    id: navTabsRow
                    visible: appWindow.width >= 900
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 4

                    Repeater {
                        model: [
                            { id: "overview", label: KI18n.i18n("Overview"), shortLabel: KI18n.i18n("Overview"), icon: "view-statistics" },
                            { id: "flight", label: KI18n.i18n("History"), shortLabel: KI18n.i18n("History"), icon: "chronometer" },
                            { id: "pressure", label: KI18n.i18n("Memory & Pressure"), shortLabel: KI18n.i18n("Memory & PSI"), icon: "memory" },
                            { id: "power", label: KI18n.i18n("Power & Cooling"), shortLabel: KI18n.i18n("Power & Fan"), icon: "battery-profile-performance" },
                            { id: "processes", label: KI18n.i18n("Processes"), shortLabel: KI18n.i18n("Processes"), icon: "view-process-all" },
                            { id: "storage", label: KI18n.i18n("Storage & Network"), shortLabel: KI18n.i18n("Storage & Net"), icon: "drive-harddisk" }
                        ]
                        delegate: QQC2.AbstractButton {
                            id: navTabBtn
                            required property var modelData
                            readonly property bool isCurrent: appWindow.currentPageName === navTabBtn.modelData.id
                            Layout.fillWidth: true
                            implicitHeight: Math.max(32, Kirigami.Theme.defaultFont.pixelSize + 16)
                            hoverEnabled: true
                            Accessible.name: navTabBtn.modelData.label + KI18n.i18n(" tab")
                            checkable: true
                            checked: navTabBtn.isCurrent
                            onClicked: appWindow.switchToPage(navTabBtn.modelData.id)

                            background: Rectangle {
                                radius: theme.innerRadius
                                color: navTabBtn.isCurrent
                                       ? (theme.isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.95))
                                       : (navTabBtn.hovered ? (theme.isDark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.5)) : "transparent")
                                border.width: 1
                                border.color: navTabBtn.activeFocus ? Kirigami.Theme.focusColor
                                              : (navTabBtn.isCurrent ? (theme.isDark ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(0, 0, 0, 0.15)) : "transparent")

                                // Active indicator line at bottom
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: navTabBtn.isCurrent ? parent.width - 20 : 0
                                    height: 2
                                    radius: 1
                                    color: Kirigami.Theme.highlightColor
                                    visible: navTabBtn.isCurrent
                                }
                            }

                            contentItem: RowLayout {
                                anchors.centerIn: parent
                                spacing: 6
                                Kirigami.Icon {
                                    source: navTabBtn.modelData.icon
                                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                    color: navTabBtn.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                                }
                                QQC2.Label {
                                    text: appWindow.width < 980 ? navTabBtn.modelData.shortLabel : navTabBtn.modelData.label
                                    font.bold: navTabBtn.isCurrent
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    color: navTabBtn.isCurrent ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                                    elide: Text.ElideRight
                                    visible: appWindow.width >= 620
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    footer: Rectangle {
        visible: telemetry.lastActionStatus.length > 0
        height: visible ? statusRow.implicitHeight + Kirigami.Units.smallSpacing * 2 : 0
        color: theme.elevatedSurfaceColor
        border.width: 1
        border.color: (telemetry.powerProfileChangePending || telemetry.actionPending)
                      ? Kirigami.Theme.neutralTextColor
                      : (telemetry.lastActionSucceeded ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor)

        RowLayout {
            id: statusRow
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: Kirigami.Units.smallSpacing

            Rectangle {
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                radius: 4
                color: (telemetry.powerProfileChangePending || telemetry.actionPending)
                       ? Kirigami.Theme.neutralTextColor
                       : (telemetry.lastActionSucceeded ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor)
            }

            QQC2.Label {
                text: ((telemetry.powerProfileChangePending || telemetry.actionPending) ? KI18n.i18n("Working: ")
                       : telemetry.lastActionSucceeded ? KI18n.i18n("Completed: ") : KI18n.i18n("Could not complete: "))
                      + telemetry.lastActionStatus
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.textColor
                font.bold: true
                Layout.fillWidth: true
            }

            QQC2.Button {
                visible: telemetry.lastActionPath.length > 0
                icon.name: "folder-open"
                text: KI18n.i18n("Open folder")
                onClicked: telemetry.openReportsFolder()
            }

            QQC2.Label {
                objectName: "recordingStatusLabel"
                Layout.maximumWidth: appWindow.width * 0.42
                wrapMode: Text.WordWrap
                text: (telemetry.sessionMode ? KI18n.i18n("View: Historical investigation") : KI18n.i18n("View: Current recording"))
                      + KI18n.i18n("\nCollector: ") + telemetry.collectorState + " · " + telemetry.pollIntervalMs + KI18n.i18n(" ms")
                      + (telemetry.recordingPaused ? KI18n.i18n(" · Recording paused in this window") : "")
                color: telemetry.collectorState.indexOf("Disconnected") === 0
                       ? Kirigami.Theme.negativeTextColor
                       : (telemetry.sessionMode || telemetry.recordingPaused || telemetry.collectorState.indexOf("Live") !== 0
                          ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.positiveTextColor)
                Accessible.name: text
                font.bold: true
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
