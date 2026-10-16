import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import com.github.loofi.hardwarepulse 1.0

Kirigami.ApplicationWindow {
    id: appWindow
    title: "Loofi Pulse — Causal Hardware & Thermal Studio"
    width: 1120
    height: 760
    minimumWidth: 860
    minimumHeight: 600

    property string initialPageName: "overview"

    PulseTelemetryController {
        id: telemetry
        pollIntervalMs: 1000
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
                text: "Causal Pulse & Topology"
                icon.name: "view-statistics"
                onTriggered: appWindow.switchToPage("overview")
            },
            Kirigami.Action {
                text: "Flight Recorder (Time Machine)"
                icon.name: "chronometer"
                onTriggered: appWindow.switchToPage("flight")
            },
            Kirigami.Action {
                text: "PSI Pressure & ZRAM Memory"
                icon.name: "memory"
                onTriggered: appWindow.switchToPage("pressure")
            },
            Kirigami.Action {
                text: "Energy & Thermal Envelope"
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
        height: visible ? 30 : 0
        color: Qt.rgba(0.07, 0.11, 0.19, 0.95)
        border.width: 1
        border.color: Qt.rgba(0.22, 0.74, 0.97, 0.35)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            QQC2.Label {
                text: "Status: " + telemetry.lastActionStatus
                color: "#38bdf8"
                font.pixelSize: 12
                font.bold: true
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: "Poll: " + telemetry.pollIntervalMs + "ms"
                      + (telemetry.recordingPaused ? " • PAUSED" : " • LIVE")
                color: telemetry.recordingPaused ? "#f59e0b" : "#10b981"
                font.pixelSize: 12
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
