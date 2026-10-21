pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

Kirigami.ScrollablePage {
    id: root

    objectName: "settingsPage"
    title: KI18n.i18n("Settings")

    required property PulseTelemetryController controller
    property var appWindow: null

    // Helper to safely get nested preferences
    readonly property var prefs: controller.collectorPreferences || ({})
    readonly property int currentMinSeverity: prefs.minNotificationSeverity !== undefined ? prefs.minNotificationSeverity : 2
    readonly property var currentMutedDomains: prefs.mutedNotificationDomains || []
    readonly property bool currentQuietHoursEnabled: !!prefs.quietHoursEnabled
    readonly property string currentQuietHoursStart: prefs.quietHoursStart || "22:00"
    readonly property string currentQuietHoursEnd: prefs.quietHoursEnd || "08:00"
    readonly property bool currentLongTermHistoryEnabled: !!prefs.longTermHistoryEnabled
    readonly property int currentHistoryRetentionDays: Number(prefs.longTermHistoryRetentionDays || 7)

    function saveCollectorPreference(key, value) {
        var copy = Object.assign({}, prefs)
        copy[key] = value
        controller.updateCollectorPreferences(copy)
    }

    function toggleMutedDomain(domainName, muted) {
        var list = (prefs.mutedNotificationDomains || []).slice()
        var idx = list.indexOf(domainName)
        if (muted && idx < 0) {
            list.push(domainName)
        } else if (!muted && idx >= 0) {
            list.splice(idx, 1)
        }
        saveCollectorPreference("mutedNotificationDomains", list)
    }

    Kirigami.FormLayout {
        id: formLayout
        anchors.left: parent.left
        anchors.right: parent.right

        // --- 1. MONITORING & SAMPLING ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Monitoring & Sampling")
        }

        QQC2.ComboBox {
            id: intervalCombo
            Kirigami.FormData.label: KI18n.i18n("Telemetry interval:")
            Accessible.name: KI18n.i18n("Telemetry sampling interval")
            model: [
                { text: KI18n.i18n("250 ms (High responsiveness)"), value: 250 },
                { text: KI18n.i18n("500 ms (Fast)"), value: 500 },
                { text: KI18n.i18n("1000 ms (Standard balanced)"), value: 1000 },
                { text: KI18n.i18n("2000 ms (Low overhead)"), value: 2000 },
                { text: KI18n.i18n("5000 ms (Minimal CPU)"), value: 5000 }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: {
                for (var i = 0; i < model.length; ++i) {
                    if (model[i].value === root.controller.pollIntervalMs) return i
                }
                return 2 // 1000 ms default
            }
            onActivated: root.controller.pollIntervalMs = model[currentIndex].value
        }

        QQC2.ComboBox {
            id: retentionCombo
            Kirigami.FormData.label: KI18n.i18n("History retention:")
            Accessible.name: KI18n.i18n("In-memory history retention duration")
            model: [
                { text: KI18n.i18n("5 minutes (Default)"), value: 300 },
                { text: KI18n.i18n("15 minutes (Extended)"), value: 900 },
                { text: KI18n.i18n("30 minutes (Deep investigation)"), value: 1800 }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: {
                for (var i = 0; i < model.length; ++i) {
                    if (model[i].value === root.controller.historyRetentionSeconds) return i
                }
                return 0 // 300s default
            }
            onActivated: root.controller.historyRetentionSeconds = model[currentIndex].value
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: KI18n.i18n("Flight recorder:")
            text: KI18n.i18n("Pause recording in this window")
            Accessible.name: KI18n.i18n("Pause Flight Recorder")
            checked: root.controller.recordingPaused
            onToggled: root.controller.recordingPaused = checked
        }

        // --- 2. BACKGROUND & AUTOSTART ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Background & Startup")
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: KI18n.i18n("Service execution:")
            text: KI18n.i18n("Continue monitoring when all windows close")
            Accessible.name: KI18n.i18n("Background monitoring toggle")
            checked: root.controller.backgroundMonitoring
            onToggled: root.controller.backgroundMonitoring = checked
        }

        QQC2.CheckBox {
            text: KI18n.i18n("Start monitoring automatically at login")
            Accessible.name: KI18n.i18n("Start monitoring at login toggle")
            checked: root.controller.startAtLogin
            onToggled: root.controller.startAtLogin = checked
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: KI18n.i18n("System Tray:")
            text: KI18n.i18n("Show system tray icon with live status")
            Accessible.name: KI18n.i18n("System tray icon toggle")
            checked: root.appWindow ? root.appWindow.savedShowTrayIcon : true
            onToggled: {
                if (root.appWindow) {
                    root.appWindow.savedShowTrayIcon = checked
                    if (root.appWindow.tray) root.appWindow.tray.visible = checked
                }
            }
        }

        QQC2.CheckBox {
            text: KI18n.i18n("Minimize to system tray when closing window")
            Accessible.name: KI18n.i18n("Minimize to tray on close toggle")
            enabled: root.appWindow ? root.appWindow.savedShowTrayIcon : true
            checked: root.appWindow ? root.appWindow.savedMinimizeToTray : true
            onToggled: {
                if (root.appWindow) {
                    root.appWindow.savedMinimizeToTray = checked
                    if (root.appWindow.tray) root.appWindow.tray.minimizeToTrayOnClose = checked
                }
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            text: KI18n.i18n("When background monitoring is active, the collector continues to detect hardware bottlenecks and power spikes quietly in the background without keeping the full window open.")
        }

        // --- 3. ALERT NOTIFICATIONS ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Alert Notifications")
        }

        QQC2.CheckBox {
            text: KI18n.i18n("Enable incident notifications in this window")
            checked: root.controller.notificationsEnabled
            onToggled: root.controller.notificationsEnabled = checked
        }

        QQC2.ComboBox {
            id: severityCombo
            Kirigami.FormData.label: KI18n.i18n("Minimum alert severity:")
            Accessible.name: KI18n.i18n("Minimum alert severity threshold")
            model: [
                { text: KI18n.i18n("Warnings & Critical (Recommended)"), value: 2 },
                { text: KI18n.i18n("Critical only (Severe throttling)"), value: 3 },
                { text: KI18n.i18n("All alerts (Including informational)"), value: 1 },
                { text: KI18n.i18n("Disabled (Mute all notifications)"), value: 4 }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: {
                for (var i = 0; i < model.length; ++i) {
                    if (model[i].value === root.currentMinSeverity) return i
                }
                return 0
            }
            onActivated: root.saveCollectorPreference("minNotificationSeverity", model[currentIndex].value)
        }

        QQC2.CheckBox {
            id: quietHoursCheck
            Kirigami.FormData.label: KI18n.i18n("Quiet hours:")
            text: KI18n.i18n("Suppress alert notifications during quiet hours")
            Accessible.name: KI18n.i18n("Quiet hours toggle")
            checked: root.currentQuietHoursEnabled
            onToggled: root.saveCollectorPreference("quietHoursEnabled", checked)
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Quiet hours schedule:")
            visible: quietHoursCheck.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: startTimeField
                objectName: "startTimeField"
                validator: RegularExpressionValidator { regularExpression: /([01][0-9]|2[0-3]):[0-5][0-9]/ }
                color: acceptableInput ? Kirigami.Theme.textColor : Kirigami.Theme.negativeTextColor
                implicitWidth: 80
                text: root.currentQuietHoursStart
                placeholderText: KI18n.i18n("22:00")
                Accessible.name: KI18n.i18n("Quiet hours start time (HH:mm)")
                onEditingFinished: { if (acceptableInput) root.saveCollectorPreference("quietHoursStart", text) }
            }

            QQC2.Label { text: KI18n.i18n("to") }

            QQC2.TextField {
                id: endTimeField
                objectName: "endTimeField"
                validator: RegularExpressionValidator { regularExpression: /([01][0-9]|2[0-3]):[0-5][0-9]/ }
                color: acceptableInput ? Kirigami.Theme.textColor : Kirigami.Theme.negativeTextColor
                implicitWidth: 80
                text: root.currentQuietHoursEnd
                placeholderText: KI18n.i18n("08:00")
                Accessible.name: KI18n.i18n("Quiet hours end time (HH:mm)")
                onEditingFinished: { if (acceptableInput) root.saveCollectorPreference("quietHoursEnd", text) }
            }
        }

        QQC2.Label {
            visible: quietHoursCheck.checked && (!startTimeField.acceptableInput || !endTimeField.acceptableInput)
            text: KI18n.i18n("Enter a valid time in HH:mm format (00:00–23:59). The last valid schedule remains active.")
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.negativeTextColor
        }

        ColumnLayout {
            Kirigami.FormData.label: KI18n.i18n("Muted alert domains:")
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: [
                    { id: "cpu", label: KI18n.i18n("CPU & SoC (Load, frequencies)") },
                    { id: "thermal", label: KI18n.i18n("Thermals & Throttling") },
                    { id: "gpu", label: KI18n.i18n("GPU (Busy, VRAM, thermals)") },
                    { id: "memory", label: KI18n.i18n("Memory & PSI (RAM, Swap, Stalls)") },
                    { id: "storage", label: KI18n.i18n("Storage & NVMe (Disk I/O, temps)") },
                    { id: "network", label: KI18n.i18n("Network (Throughput spikes)") },
                    { id: "battery", label: KI18n.i18n("Battery & Power (High discharge)") }
                ]

                delegate: QQC2.CheckBox {
                    required property var modelData
                    text: modelData.label
                    checked: (root.currentMutedDomains || []).indexOf(modelData.id) >= 0
                    Accessible.name: KI18n.i18n("Mute alerts for ") + modelData.label
                    onToggled: root.toggleMutedDomain(modelData.id, checked)
                }
            }
        }

        // --- 4. DISPLAY & UNITS ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Display & Units")
        }

        QQC2.ComboBox {
            id: tempUnitCombo
            Kirigami.FormData.label: KI18n.i18n("Temperature unit:")
            Accessible.name: KI18n.i18n("Temperature unit preference")
            model: [
                { text: KI18n.i18n("Celsius (°C)"), value: "C" },
                { text: KI18n.i18n("Fahrenheit (°F)"), value: "F" }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: root.controller.temperatureUnit === "F" ? 1 : 0
            onActivated: root.controller.temperatureUnit = model[currentIndex].value
        }

        // --- 5. PRIVACY & DATA STORAGE ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Privacy & Data Storage")
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: KI18n.i18n("Process details:")
            text: KI18n.i18n("Include process names and PIDs in exported session reports")
            Accessible.name: KI18n.i18n("Include process details in exports")
            checked: root.controller.exportIncludeProcessDetails
            onToggled: root.controller.exportIncludeProcessDetails = checked
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: KI18n.i18n("Long-term history:")
            text: KI18n.i18n("Enable persistent metrics history across restarts (opt-in)")
            Accessible.name: KI18n.i18n("Enable persistent long-term metrics history")
            checked: root.currentLongTermHistoryEnabled
            onToggled: root.saveCollectorPreference("longTermHistoryEnabled", checked)
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: Boolean(root.prefs.historyStoreError)
            type: Kirigami.MessageType.Error
            text: root.prefs.historyStoreError || ""
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: Number(root.prefs.historyStoreLegacyIncidentCount || 0) > 0
            type: Kirigami.MessageType.Warning
            text: KI18n.i18n("Older incident summaries without calendar timestamps are retained but excluded from time-based trends.")
        }

        QQC2.ComboBox {
            id: longTermRetentionCombo
            visible: root.currentLongTermHistoryEnabled
            Kirigami.FormData.label: KI18n.i18n("Retention period:")
            Accessible.name: KI18n.i18n("Long term history retention period")
            model: [
                { text: KI18n.i18n("1 Day"), value: 1 },
                { text: KI18n.i18n("7 Days (Recommended)"), value: 7 },
                { text: KI18n.i18n("14 Days"), value: 14 },
                { text: KI18n.i18n("30 Days"), value: 30 }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: {
                var d = root.currentHistoryRetentionDays || 7
                if (d <= 1) return 0
                if (d <= 7) return 1
                if (d <= 14) return 2
                return 3
            }
            onActivated: root.saveCollectorPreference("longTermHistoryRetentionDays", model[currentIndex].value)
        }

        RowLayout {
            visible: root.currentLongTermHistoryEnabled
            Kirigami.FormData.label: KI18n.i18n("Storage usage:")
            spacing: Kirigami.Units.mediumSpacing

            QQC2.Label {
                text: {
                    var bytes = Number(root.prefs.historyStoreBytes || 0)
                    if (bytes < 1024) return bytes + KI18n.i18n(" B")
                    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + KI18n.i18n(" KiB")
                    return (bytes / 1048576).toFixed(2) + KI18n.i18n(" MiB")
                }
            }

            PulseButton {
                text: KI18n.i18n("Clear Database")
                icon.name: "edit-delete"
                onClicked: clearDatabaseDialog.open()
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Information
            text: KI18n.i18n("The history database only records 1-minute numerical performance aggregates (min, avg, max) for CPU, GPU, memory, disks, and thermals. Process identities, command lines, window titles, and user PIDs are not stored in this database. Session files may include process identities when the export option above is enabled.")
        }

        // --- 6. NETWORK INTERFACES ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Network Interfaces")
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: KI18n.i18n("Interface selection:")
            text: KI18n.i18n("Automatically use physical interfaces")
            checked: root.controller.selectedNetworkInterfaces.length === 0
            Accessible.name: KI18n.i18n("Automatically use physical network interfaces")
            onToggled: {
                if (checked) root.controller.selectedNetworkInterfaces = []
                else {
                    var ids = []
                    for (var i = 0; i < root.controller.networkInterfaces.length; ++i) {
                        var iface = root.controller.networkInterfaces[i]
                        if (iface.physical && iface.available) ids.push(iface.id)
                    }
                    if (ids.length > 0) root.controller.selectedNetworkInterfaces = ids
                }
            }
        }

        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: root.controller.networkInterfaces
                delegate: QQC2.CheckBox {
                    required property var modelData
                    text: modelData.name + (modelData.physical ? KI18n.i18n(" (physical)") : KI18n.i18n(" (virtual)"))
                          + (modelData.available ? "" : KI18n.i18n(" · unavailable"))
                    checked: modelData.included
                    Accessible.name: KI18n.i18n("Monitor interface ") + modelData.name
                    onToggled: {
                        var ids = root.controller.selectedNetworkInterfaces.slice()
                        if (ids.length === 0) {
                            for (var i = 0; i < root.controller.networkInterfaces.length; ++i) {
                                var candidate = root.controller.networkInterfaces[i]
                                if (candidate.included) ids.push(candidate.id)
                            }
                        }
                        var index = ids.indexOf(modelData.id)
                        if (checked && index < 0) ids.push(modelData.id)
                        else if (!checked && index >= 0) ids.splice(index, 1)
                        root.controller.selectedNetworkInterfaces = ids
                    }
                }
            }
        }

        // --- 7. ABOUT & HELP ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("About & Documentation")
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Help & Diagnostics:")
            spacing: Kirigami.Units.mediumSpacing

            QQC2.Button {
                text: KI18n.i18n("About Loofi Pulse")
                icon.name: "help-about"
                Accessible.name: KI18n.i18n("Open About Loofi Pulse dialog")
                onClicked: {
                    if (root.appWindow && typeof root.appWindow.openAboutDialog === "function") {
                        root.appWindow.openAboutDialog()
                    }
                }
            }

            QQC2.Button {
                text: KI18n.i18n("Keyboard Shortcuts")
                icon.name: "input-keyboard"
                Accessible.name: KI18n.i18n("Open Keyboard Shortcuts cheat sheet")
                onClicked: {
                    if (root.appWindow && typeof root.appWindow.openShortcutsDialog === "function") {
                        root.appWindow.openShortcutsDialog()
                    }
                }
            }
        }
    }
    QQC2.Dialog {
        id: clearDatabaseDialog
        objectName: "clearDatabaseDialog"
        title: KI18n.i18n("Clear persistent history?")
        anchors.centerIn: parent
        modal: true
        standardButtons: QQC2.Dialog.Cancel | QQC2.Dialog.Ok
        onAccepted: root.controller.clearPersistedHistory()
        QQC2.Label { text: KI18n.i18n("This permanently removes stored trends and incident summaries. Saved session files are kept."); wrapMode: Text.WordWrap; width: Math.min(root.width - 60, 420) }
    }

}
