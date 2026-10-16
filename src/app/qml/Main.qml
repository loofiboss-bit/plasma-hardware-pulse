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
        appWindow.pageStack.clear()
        if (name === "flight") {
            appWindow.pageStack.push(flightComp)
        } else if (name === "pressure") {
            appWindow.pageStack.push(pressureComp)
        } else if (name === "power") {
            appWindow.pageStack.push(powerComp)
        } else {
            appWindow.pageStack.push(overviewComp)
        }
    }

    onInitialPageNameChanged: {
        switchToPage(initialPageName)
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
            }
        ]
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
