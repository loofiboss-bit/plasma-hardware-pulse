pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Qt.labs.settings
import com.github.loofi.hardwarepulse 1.0

Kirigami.ApplicationWindow {
    id: appWindow
    objectName: "pulseWindow"
    title: "Loofi Pulse"
    width: 1120
    height: 760
    minimumWidth: 600
    minimumHeight: 480

    property string initialPageName: "overview"

    Settings {
        id: appSettings
        category: "LoofiPulse"
        property int savedPollInterval: 1000
        property bool savedNotificationsEnabled: true
    }

    PulseTelemetryController {
        id: telemetry
        pollIntervalMs: appSettings.savedPollInterval
        notificationsEnabled: appSettings.savedNotificationsEnabled
        onPollIntervalChanged: appSettings.savedPollInterval = pollIntervalMs
        onNotificationsEnabledChanged: appSettings.savedNotificationsEnabled = notificationsEnabled
    }

    function switchToPage(name) {
        var targetComp = overviewComp
        if (name === "flight") {
            targetComp = flightComp
        } else if (name === "pressure") {
            targetComp = pressureComp
        } else if (name === "power") {
            targetComp = powerComp
        }
        if (appWindow.pageStack.depth > 0) {
            appWindow.pageStack.replace(targetComp)
        } else {
            appWindow.pageStack.push(targetComp)
        }
    }

    onInitialPageNameChanged: {
        if (initialPageName !== "overview") {
            switchToPage(initialPageName)
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
                onTriggered: appWindow.switchToPage("overview")
            },
            Kirigami.Action {
                objectName: "navigateHistory"
                text: "History"
                icon.name: "chronometer"
                onTriggered: appWindow.switchToPage("flight")
            },
            Kirigami.Action {
                objectName: "navigatePressure"
                text: "Memory & Pressure"
                icon.name: "memory"
                onTriggered: appWindow.switchToPage("pressure")
            },
            Kirigami.Action {
                objectName: "navigatePower"
                text: "Power & Cooling"
                icon.name: "battery-profile-performance"
                onTriggered: appWindow.switchToPage("power")
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
                text: telemetry.notificationsEnabled ? "Disable notifications" : "Enable notifications"
                icon.name: telemetry.notificationsEnabled ? "notifications-disabled" : "notifications"
                onTriggered: telemetry.notificationsEnabled = !telemetry.notificationsEnabled
            },
            Kirigami.Action {
                text: "Copy Incident Report (MD)"
                icon.name: "edit-copy"
                onTriggered: telemetry.copyIncidentReportToClipboard()
            },
            Kirigami.Action {
                text: "Poll Rate (" + telemetry.pollIntervalMs + " ms)"
                icon.name: "speedometer"
                Kirigami.Action {
                    text: "Fast (500 ms)"
                    onTriggered: telemetry.pollIntervalMs = 500
                }
                Kirigami.Action {
                    text: "Standard (1000 ms)"
                    onTriggered: telemetry.pollIntervalMs = 1000
                }
                Kirigami.Action {
                    text: "Eco (2000 ms)"
                    onTriggered: telemetry.pollIntervalMs = 2000
                }
            }
        ]
    }

    footer: Rectangle {
        visible: telemetry.lastActionStatus.length > 0
        height: visible ? statusRow.implicitHeight + Kirigami.Units.smallSpacing * 2 : 0
        color: Kirigami.Theme.backgroundColor
        border.width: 1
        border.color: Kirigami.Theme.focusColor

        RowLayout {
            id: statusRow
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            QQC2.Label {
                text: (telemetry.powerProfileChangePending ? "Working: "
                       : telemetry.lastActionSucceeded ? "Completed: " : "Could not complete: ")
                      + telemetry.lastActionStatus
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.textColor
                font.bold: true
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: "Poll: " + telemetry.pollIntervalMs + "ms"
                      + (telemetry.recordingPaused ? " • PAUSED" : " • LIVE")
                color: telemetry.recordingPaused ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.positiveTextColor
                font.bold: true
            }
        }
    }

    Component {
        id: overviewComp
        OverviewPage {
            controller: telemetry
        }
    }

    Component {
        id: flightComp
        FlightRecorderPage {
            controller: telemetry
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

    pageStack.initialPage: overviewComp
}
