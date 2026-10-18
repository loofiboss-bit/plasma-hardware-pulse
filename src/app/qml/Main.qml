pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import QtCore as QtCore
import com.github.loofi.hardwarepulse 1.0

Kirigami.ApplicationWindow {
    id: appWindow
    objectName: "pulseWindow"
    title: "Loofi Pulse"
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
    }

    PulseTelemetryController {
        id: telemetry
        objectName: "telemetryController"
        pollIntervalMs: appSettings.savedPollInterval
        historyRetentionSeconds: appSettings.savedHistoryRetentionSeconds
        notificationsEnabled: appSettings.savedNotificationsEnabled
        selectedNetworkInterfaces: appSettings.savedNetworkInterfaces
        exportIncludeProcessDetails: appSettings.savedExportIncludeProcessDetails
        onSelectedNetworkInterfacesChanged: appSettings.savedNetworkInterfaces = selectedNetworkInterfaces
        onExportIncludeProcessDetailsChanged: appSettings.savedExportIncludeProcessDetails = exportIncludeProcessDetails
        onPollIntervalChanged: appSettings.savedPollInterval = pollIntervalMs
        onHistoryRetentionChanged: appSettings.savedHistoryRetentionSeconds = historyRetentionSeconds
        onNotificationsEnabledChanged: appSettings.savedNotificationsEnabled = notificationsEnabled
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

    FileDialog {
        id: openSessionDialog
        objectName: "openSessionDialog"
        title: "Open Pulse session"
        nameFilters: ["Pulse sessions (*.pulse-session)"]
        fileMode: FileDialog.OpenFile
        onAccepted: { telemetry.openSession(selectedFile.toString()); appWindow.switchToPage("flight") }
    }
    FileDialog {
        id: saveSessionDialog
        objectName: "saveSessionDialog"
        title: "Save Pulse session"
        nameFilters: ["Pulse sessions (*.pulse-session)"]
        defaultSuffix: "pulse-session"
        fileMode: FileDialog.SaveFile
        onAccepted: telemetry.saveSession(selectedFile.toString())
    }
    onClosing: close => {
        if (!telemetry.canQuitWithoutSaving()) {
            close.accepted = false
            unsavedDialog.open()
        }
    }
    QQC2.Dialog {
        id: unsavedDialog
        objectName: "unsavedInvestigationDialog"
        title: "Save this investigation?"
        modal: true
        anchors.centerIn: parent
        width: Math.min(appWindow.width - 40, 480)
        standardButtons: QQC2.Dialog.Cancel
        ColumnLayout {
            width: parent.width
            QQC2.Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "This investigation has not been saved. Save a session to keep the original evidence before closing the last monitor." }
            QQC2.Button { text: "Save session…"; onClicked: { unsavedDialog.close(); saveSessionDialog.open() } }
            QQC2.Button { text: "Discard investigation and close"; onClicked: { telemetry.returnToLive(); appWindow.close() } }
        }
    }

    function openSession(path) { telemetry.openSession(path); switchToPage("flight") }
    function openIncident(id) { telemetry.selectIncident(id); switchToPage("flight", "inspector") }
    function navigateTo(name, section) { switchToPage(name, section) }
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
        title: "Settings"
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
                QQC2.Label { text: "Sampling interval (ms)"; font.bold: true }
                QQC2.SpinBox {
                    objectName: "samplingIntervalSetting"
                    from: 250; to: 10000; stepSize: 250
                    value: telemetry.pollIntervalMs
                    onValueModified: telemetry.pollIntervalMs = value
                    Accessible.name: "Sampling interval in milliseconds"
                }
                QQC2.Label { text: "History retention"; font.bold: true }
                QQC2.ComboBox {
                    objectName: "historyRetentionSetting"
                    model: ["5 minutes", "15 minutes", "30 minutes"]
                    currentIndex: telemetry.historyRetentionSeconds === 1800 ? 2 : telemetry.historyRetentionSeconds === 900 ? 1 : 0
                    onActivated: telemetry.historyRetentionSeconds = [300, 900, 1800][currentIndex]
                    Accessible.name: "History retention"
                }
                QQC2.CheckBox {
                    text: "Enable incident notifications"
                    checked: telemetry.notificationsEnabled
                    onToggled: telemetry.notificationsEnabled = checked
                }
                QQC2.CheckBox {
                    text: "Include process names and IDs in reports"
                    checked: telemetry.exportIncludeProcessDetails
                    onToggled: telemetry.exportIncludeProcessDetails = checked
                }
                QQC2.CheckBox {
                    text: "Continue monitoring when all windows close"
                    checked: telemetry.backgroundMonitoring
                    onToggled: telemetry.backgroundMonitoring = checked
                }
                QQC2.CheckBox {
                    text: "Start monitoring at login"
                    checked: telemetry.startAtLogin
                    onToggled: telemetry.startAtLogin = checked
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "Background monitoring and login startup are separate opt-ins. Recordings stay in memory until you save a session. The process-detail privacy setting also applies to sessions."
                }
                QQC2.Label { text: "Network interfaces"; font.bold: true }
                QQC2.CheckBox {
                    text: "Automatically use physical interfaces"
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
                        text: modelData.name + (modelData.physical ? " (physical)" : " (virtual)")
                              + (modelData.available ? "" : " · unavailable")
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
                    text: "An empty selection uses physical interfaces automatically. Reports include process details only when explicitly enabled."
                }
            }
        }
    }

    globalDrawer: Kirigami.GlobalDrawer {
        title: "Loofi Pulse"
        titleIcon: "utilities-system-monitor"
        isMenu: false

        actions: [
            Kirigami.Action {
                objectName: "navigateOverview"
                text: "Overview"
                icon.name: "view-statistics"
                checked: appWindow.currentPageName === "overview"
                onTriggered: appWindow.switchToPage("overview")
            },
            Kirigami.Action {
                objectName: "navigateHistory"
                text: "History"
                icon.name: "chronometer"
                checked: appWindow.currentPageName === "flight"
                onTriggered: appWindow.switchToPage("flight")
            },
            Kirigami.Action {
                objectName: "navigatePressure"
                text: "Memory & Pressure"
                icon.name: "memory"
                checked: appWindow.currentPageName === "pressure"
                onTriggered: appWindow.switchToPage("pressure")
            },
            Kirigami.Action {
                objectName: "navigatePower"
                text: "Power & Cooling"
                icon.name: "battery-profile-performance"
                checked: appWindow.currentPageName === "power"
                onTriggered: appWindow.switchToPage("power")
            },
            Kirigami.Action {
                objectName: "navigateProcesses"
                text: "Processes"
                icon.name: "view-process-all"
                checked: appWindow.currentPageName === "processes"
                onTriggered: appWindow.switchToPage("processes")
            },
            Kirigami.Action {
                objectName: "navigateStorage"
                text: "Storage & Network"
                icon.name: "drive-harddisk"
                checked: appWindow.currentPageName === "storage"
                onTriggered: appWindow.switchToPage("storage")
            },
            Kirigami.Action {
                text: "Open session…"
                icon.name: "document-open"
                onTriggered: openSessionDialog.open()
            },
            Kirigami.Action {
                text: "Save session…"
                icon.name: "document-save-as"
                onTriggered: saveSessionDialog.open()
            },
            Kirigami.Action {
                text: "Snooze notifications for 15 minutes"
                icon.name: "notifications-disabled"
                onTriggered: telemetry.snoozeNotifications()
            },
            Kirigami.Action {
                separator: true
            },
            Kirigami.Action {
                text: telemetry.recordingPaused ? "Resume Flight Recorder" : "Pause Flight Recorder"
                icon.name: telemetry.recordingPaused ? "media-playback-start" : "media-playback-pause"
                onTriggered: telemetry.recordingPaused = !telemetry.recordingPaused
            },
            Kirigami.Action {
                objectName: "openSettings"
                text: "Settings"
                icon.name: "configure"
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
                text: "Collector service is disconnected (" + telemetry.collectorState + "). Real-time telemetry is paused."
                actions: [
                    Kirigami.Action {
                        text: "Reconnect"
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
                            text: "· " + telemetry.verdictHeadline
                            color: Kirigami.Theme.textColor
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    QQC2.AbstractButton {
                        objectName: "statusExplanationButton"
                        anchors.fill: parent
                        Accessible.name: telemetry.verdictHeadline + ". Open status explanation"
                        onClicked: appWindow.switchToPage("overview", "status")
                        background: Rectangle { color: "transparent"; border.width: parent.activeFocus ? 2 : 0; border.color: Kirigami.Theme.focusColor }
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
                                  ? Math.round(telemetry.cpuPackageTemp) + "°C"
                                  : "Temp N/A"
                            color: theme.tempAccent
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }

                        QQC2.Label {
                            text: "·"
                            color: Kirigami.Theme.disabledTextColor
                        }

                        QQC2.Label {
                            text: telemetry.cpuLoadAvailable
                                  ? Math.round(telemetry.cpuLoadPercent) + "% CPU"
                                  : "CPU N/A"
                            color: theme.cpuAccent
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }

                        QQC2.Label {
                            text: "·"
                            color: Kirigami.Theme.disabledTextColor
                        }

                        QQC2.Label {
                            text: telemetry.psiAvailable
                                  ? "Responsiveness " + Math.round(telemetry.responsivenessScore) + "/100"
                                  : "Index unavailable"
                            color: theme.psiAccent
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }

                        QQC2.Label {
                            visible: telemetry.packagePowerAvailable
                            text: "· CPU package " + telemetry.packageWatts.toFixed(1) + " W"
                                  + (telemetry.packagePowerEstimated ? " (est.)" : "")
                            color: theme.powerAccent
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }
                }

                // Live / Pause pill button
                PulseButton {
                    icon.name: telemetry.recordingPaused ? "media-playback-start" : "media-playback-pause"
                    text: telemetry.recordingPaused ? "Recording paused" : (telemetry.collectorState + " · " + telemetry.pollIntervalMs + " ms")
                    display: appWindow.width < 680 ? QQC2.AbstractButton.IconOnly : QQC2.AbstractButton.TextBesideIcon
                    Accessible.name: telemetry.recordingPaused ? "Resume Flight Recorder" : "Pause Flight Recorder"
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
                            text: "Collector: " + telemetry.collectorState
                                  + (telemetry.sessionMode ? " · HISTORICAL investigation (live monitoring continues separately)" : " · Live recording")
                                  + (telemetry.unsavedInvestigation ? " · Unsaved" : "")
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
                implicitHeight: navTabsRow.implicitHeight + 8
                radius: theme.innerRadius + 2
                color: theme.cardNestedBg
                border.width: 1
                border.color: theme.subtleBorderColor

                RowLayout {
                    id: navTabsRow
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 4

                    Repeater {
                        model: [
                            { id: "overview", label: "Overview", shortLabel: "Overview", icon: "view-statistics" },
                            { id: "flight", label: "History", shortLabel: "History", icon: "chronometer" },
                            { id: "pressure", label: "Memory & Pressure", shortLabel: "Memory & PSI", icon: "memory" },
                            { id: "power", label: "Power & Cooling", shortLabel: "Power & Fan", icon: "battery-profile-performance" },
                            { id: "processes", label: "Processes", shortLabel: "Processes", icon: "view-process-all" },
                            { id: "storage", label: "Storage & Network", shortLabel: "Storage & Net", icon: "drive-harddisk" }
                        ]
                        delegate: QQC2.AbstractButton {
                            id: navTabBtn
                            required property var modelData
                            readonly property bool isCurrent: appWindow.currentPageName === navTabBtn.modelData.id
                            Layout.fillWidth: true
                            implicitHeight: 32
                            hoverEnabled: true
                            Accessible.name: navTabBtn.modelData.label + " tab"
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
                text: ((telemetry.powerProfileChangePending || telemetry.actionPending) ? "Working: "
                       : telemetry.lastActionSucceeded ? "Completed: " : "Could not complete: ")
                      + telemetry.lastActionStatus
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.textColor
                font.bold: true
                Layout.fillWidth: true
            }

            QQC2.Button {
                visible: telemetry.lastActionPath.length > 0
                icon.name: "folder-open"
                text: "Open folder"
                onClicked: telemetry.openReportsFolder()
            }

            QQC2.Label {
                objectName: "recordingStatusLabel"
                Layout.maximumWidth: appWindow.width * 0.42
                wrapMode: Text.WordWrap
                text: (telemetry.sessionMode ? "View: Historical investigation" : "View: Current recording")
                      + "\nCollector: " + telemetry.collectorState + " · " + telemetry.pollIntervalMs + " ms"
                      + (telemetry.recordingPaused ? " · Recording paused in this window" : "")
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

    pageStack.initialPage: overviewComp
}
