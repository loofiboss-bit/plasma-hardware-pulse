pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

Kirigami.ScrollablePage {
    id: root

    title: "Settings"

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
            Kirigami.FormData.label: "Monitoring & Sampling"
        }

        QQC2.ComboBox {
            id: intervalCombo
            Kirigami.FormData.label: "Telemetry interval:"
            Accessible.name: "Telemetry sampling interval"
            model: [
                { text: "250 ms (High responsiveness)", value: 250 },
                { text: "500 ms (Fast)", value: 500 },
                { text: "1000 ms (Standard balanced)", value: 1000 },
                { text: "2000 ms (Low overhead)", value: 2000 },
                { text: "5000 ms (Minimal CPU)", value: 5000 }
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
            Kirigami.FormData.label: "History retention:"
            Accessible.name: "In-memory history retention duration"
            model: [
                { text: "1 minute (Diagnostic)", value: 60 },
                { text: "5 minutes (Default)", value: 300 },
                { text: "15 minutes (Extended)", value: 900 },
                { text: "30 minutes (Deep investigation)", value: 1800 }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: {
                for (var i = 0; i < model.length; ++i) {
                    if (model[i].value === root.controller.historyRetentionSeconds) return i
                }
                return 1 // 300s default
            }
            onActivated: root.controller.historyRetentionSeconds = model[currentIndex].value
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: "Flight recorder:"
            text: "Pause recording in this window"
            Accessible.name: "Pause Flight Recorder"
            checked: root.controller.recordingPaused
            onToggled: root.controller.recordingPaused = checked
        }

        // --- 2. BACKGROUND & AUTOSTART ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Background & Startup"
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: "Service execution:"
            text: "Continue monitoring when all windows close"
            Accessible.name: "Background monitoring toggle"
            checked: root.controller.backgroundMonitoring
            onToggled: root.controller.backgroundMonitoring = checked
        }

        QQC2.CheckBox {
            text: "Start monitoring automatically at login"
            Accessible.name: "Start monitoring at login toggle"
            checked: root.controller.startAtLogin
            onToggled: root.controller.startAtLogin = checked
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            text: "When background monitoring is active, the collector continues to detect hardware bottlenecks and power spikes quietly in the background without keeping the full window open."
        }

        // --- 3. ALERT NOTIFICATIONS ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Alert Notifications"
        }

        QQC2.ComboBox {
            id: severityCombo
            Kirigami.FormData.label: "Minimum alert severity:"
            Accessible.name: "Minimum alert severity threshold"
            model: [
                { text: "Warnings & Critical (Recommended)", value: 2 },
                { text: "Critical only (Severe throttling)", value: 3 },
                { text: "All alerts (Including informational)", value: 1 },
                { text: "Disabled (Mute all notifications)", value: 4 }
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
            Kirigami.FormData.label: "Quiet hours:"
            text: "Suppress alert notifications during quiet hours"
            Accessible.name: "Quiet hours toggle"
            checked: root.currentQuietHoursEnabled
            onToggled: root.saveCollectorPreference("quietHoursEnabled", checked)
        }

        RowLayout {
            Kirigami.FormData.label: "Quiet hours schedule:"
            visible: quietHoursCheck.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: startTimeField
                implicitWidth: 80
                text: root.currentQuietHoursStart
                placeholderText: "22:00"
                Accessible.name: "Quiet hours start time (HH:mm)"
                onEditingFinished: root.saveCollectorPreference("quietHoursStart", text)
            }

            QQC2.Label { text: "to" }

            QQC2.TextField {
                id: endTimeField
                implicitWidth: 80
                text: root.currentQuietHoursEnd
                placeholderText: "08:00"
                Accessible.name: "Quiet hours end time (HH:mm)"
                onEditingFinished: root.saveCollectorPreference("quietHoursEnd", text)
            }
        }

        ColumnLayout {
            Kirigami.FormData.label: "Muted alert domains:"
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: [
                    { id: "cpu", label: "CPU & SoC (Load, frequencies)" },
                    { id: "thermal", label: "Thermals & Throttling" },
                    { id: "gpu", label: "GPU (Busy, VRAM, thermals)" },
                    { id: "memory", label: "Memory & PSI (RAM, Swap, Stalls)" },
                    { id: "storage", label: "Storage & NVMe (Disk I/O, temps)" },
                    { id: "network", label: "Network (Throughput spikes)" },
                    { id: "battery", label: "Battery & Power (High discharge)" }
                ]

                delegate: QQC2.CheckBox {
                    required property var modelData
                    text: modelData.label
                    checked: (root.currentMutedDomains || []).indexOf(modelData.id) >= 0
                    Accessible.name: "Mute alerts for " + modelData.label
                    onToggled: root.toggleMutedDomain(modelData.id, checked)
                }
            }
        }

        // --- 4. DISPLAY & UNITS ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Display & Units"
        }

        QQC2.ComboBox {
            id: tempUnitCombo
            Kirigami.FormData.label: "Temperature unit:"
            Accessible.name: "Temperature unit preference"
            model: [
                { text: "Celsius (°C)", value: "C" },
                { text: "Fahrenheit (°F)", value: "F" }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: root.controller.temperatureUnit === "F" ? 1 : 0
            onActivated: root.controller.temperatureUnit = model[currentIndex].value
        }

        // --- 5. PRIVACY & DATA STORAGE ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Privacy & Data Storage"
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: "Process details:"
            text: "Include process names and PIDs in exported session reports"
            Accessible.name: "Include process details in exports"
            checked: root.controller.exportIncludeProcessDetails
            onToggled: root.controller.exportIncludeProcessDetails = checked
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: "Long-term history:"
            text: "Enable persistent metrics history across restarts (opt-in)"
            Accessible.name: "Enable persistent long-term metrics history"
            checked: root.currentLongTermHistoryEnabled
            onToggled: root.saveCollectorPreference("longTermHistoryEnabled", checked)
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Information
            text: "Privacy Guarantee: Persistent history only records 1-minute numerical performance aggregates (min, avg, max) for CPU, GPU, memory, disks, and thermals. Process identities, command lines, window titles, and user PIDs are NEVER persisted or stored."
        }

        // --- 6. NETWORK INTERFACES ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Network Interfaces"
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: "Interface selection:"
            text: "Automatically use physical interfaces"
            checked: root.controller.selectedNetworkInterfaces.length === 0
            Accessible.name: "Automatically use physical network interfaces"
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
                    text: modelData.name + (modelData.physical ? " (physical)" : " (virtual)")
                          + (modelData.available ? "" : " · unavailable")
                    checked: modelData.included
                    Accessible.name: "Monitor interface " + modelData.name
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
            Kirigami.FormData.label: "About & Documentation"
        }

        RowLayout {
            Kirigami.FormData.label: "Help & Diagnostics:"
            spacing: Kirigami.Units.mediumSpacing

            QQC2.Button {
                text: "About Loofi Pulse"
                icon.name: "help-about"
                Accessible.name: "Open About Loofi Pulse dialog"
                onClicked: {
                    if (root.appWindow && typeof root.appWindow.openAboutDialog === "function") {
                        root.appWindow.openAboutDialog()
                    }
                }
            }

            QQC2.Button {
                text: "Keyboard Shortcuts"
                icon.name: "input-keyboard"
                Accessible.name: "Open Keyboard Shortcuts cheat sheet"
                onClicked: {
                    if (root.appWindow && typeof root.appWindow.openShortcutsDialog === "function") {
                        root.appWindow.openShortcutsDialog()
                    }
                }
            }
        }
    }
}
