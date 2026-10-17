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
    width: 1160
    height: 780
    minimumWidth: 600
    minimumHeight: 480

    property string initialPageName: "overview"
    property string currentPageName: "overview"

    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: appWindow.Kirigami.Theme.backgroundColor
        disabledColor: appWindow.Kirigami.Theme.disabledTextColor
        negativeColor: appWindow.Kirigami.Theme.negativeTextColor
        neutralColor: appWindow.Kirigami.Theme.neutralTextColor
        positiveColor: appWindow.Kirigami.Theme.positiveTextColor
    }

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
        var normalized = "overview"
        if (name === "flight") {
            targetComp = flightComp
            normalized = "flight"
        } else if (name === "pressure") {
            targetComp = pressureComp
            normalized = "pressure"
        } else if (name === "power") {
            targetComp = powerComp
            normalized = "power"
        }
        appWindow.currentPageName = normalized
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

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appWindow.switchToPage("overview")
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // Quick telemetry readout pills on wide screens
                Rectangle {
                    visible: appWindow.width >= 720
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
                                  ? Math.round(telemetry.responsivenessScore) + "% PSI"
                                  : "PSI N/A"
                            color: theme.psiAccent
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }

                        QQC2.Label {
                            visible: telemetry.packagePowerAvailable
                            text: "· " + telemetry.packageWatts.toFixed(1) + "W"
                            color: theme.powerAccent
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }
                }

                // Live / Pause pill button
                QQC2.ToolButton {
                    icon.name: telemetry.recordingPaused ? "media-playback-start" : "media-playback-pause"
                    text: telemetry.recordingPaused ? "PAUSED" : ("LIVE · " + telemetry.pollIntervalMs + "ms")
                    display: appWindow.width < 680 ? QQC2.AbstractButton.IconOnly : QQC2.AbstractButton.TextBesideIcon
                    Accessible.name: telemetry.recordingPaused ? "Resume Flight Recorder" : "Pause Flight Recorder"
                    onClicked: telemetry.recordingPaused = !telemetry.recordingPaused
                }
            }

            // Segmented View Switcher Row
            Flow {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: [
                        { id: "overview", label: "Overview", icon: "view-statistics" },
                        { id: "flight", label: "History", icon: "chronometer" },
                        { id: "pressure", label: "Memory & Pressure", icon: "memory" },
                        { id: "power", label: "Power & Cooling", icon: "battery-profile-performance" }
                    ]
                    delegate: QQC2.AbstractButton {
                        id: navTabBtn
                        required property var modelData
                        readonly property bool isCurrent: appWindow.currentPageName === navTabBtn.modelData.id
                        implicitWidth: tabContentRow.implicitWidth + 18
                        implicitHeight: tabContentRow.implicitHeight + 10
                        hoverEnabled: true
                        Accessible.name: navTabBtn.modelData.label + " tab"
                        onClicked: appWindow.switchToPage(navTabBtn.modelData.id)

                        background: Rectangle {
                            radius: theme.innerRadius
                            color: navTabBtn.isCurrent
                                   ? theme.accentTint(Kirigami.Theme.highlightColor, theme.isDark ? 0.22 : 0.14)
                                   : (navTabBtn.hovered ? theme.elevatedSurfaceColor : "transparent")
                            border.width: navTabBtn.isCurrent ? 1.5 : 1
                            border.color: navTabBtn.isCurrent
                                          ? Kirigami.Theme.highlightColor
                                          : (navTabBtn.hovered ? theme.cardBorderColor : "transparent")
                        }

                        contentItem: RowLayout {
                            id: tabContentRow
                            spacing: 6
                            Kirigami.Icon {
                                source: navTabBtn.modelData.icon
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                color: navTabBtn.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                            }
                            QQC2.Label {
                                text: navTabBtn.modelData.label
                                font.bold: navTabBtn.isCurrent
                                color: navTabBtn.isCurrent ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
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
        border.color: telemetry.powerProfileChangePending
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
                color: telemetry.powerProfileChangePending
                       ? Kirigami.Theme.neutralTextColor
                       : (telemetry.lastActionSucceeded ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor)
            }

            QQC2.Label {
                text: (telemetry.powerProfileChangePending ? "Working: "
                       : telemetry.lastActionSucceeded ? "Completed: " : "Could not complete: ")
                      + telemetry.lastActionStatus
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.textColor
                font.bold: true
                Layout.fillWidth: true
            }

            QQC2.Button {
                visible: telemetry.lastActionSucceeded && telemetry.lastActionPath.length > 0
                icon.name: "folder-open"
                text: "Open folder"
                onClicked: telemetry.openReportsFolder()
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
            onNavigateRequested: (pageName) => appWindow.switchToPage(pageName)
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
