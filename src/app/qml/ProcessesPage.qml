pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    objectName: "processesPage"
    title: "Processes"
    titleDelegate: Item {
        width: parent ? parent.width : 0
        height: 0
        visible: false
    }
    required property var controller
    property string selectedIdentity: ""
    property var lastSelection: null

    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: page.Kirigami.Theme.backgroundColor
        disabledColor: page.Kirigami.Theme.disabledTextColor
        negativeColor: page.Kirigami.Theme.negativeTextColor
        neutralColor: page.Kirigami.Theme.neutralTextColor
        positiveColor: page.Kirigami.Theme.positiveTextColor
    }

    readonly property var visibleProcesses: {
        var query = search.text.trim().toLowerCase()
        var rows = page.controller.allProcesses.filter(function(row) {
            return !query || (String(row.name) + " " + row.pid + " " + row.owner).toLowerCase().indexOf(query) >= 0
        })
        rows.sort(function(a, b) {
            var diff = sort.currentIndex === 0 ? Number(b.cpuPercent) - Number(a.cpuPercent)
                     : sort.currentIndex === 1 ? Number(b.rssMegabytes) - Number(a.rssMegabytes)
                     : String(a.name).localeCompare(String(b.name))
            return diff || Number(a.pid) - Number(b.pid)
        })
        return rows
    }

    readonly property var selectedProcess: {
        var rows = page.controller.allProcesses
        for (var i = 0; i < rows.length; ++i)
            if (page.identity(rows[i]) === page.selectedIdentity) return rows[i]
        return null
    }

    onSelectedProcessChanged: { if (selectedProcess) lastSelection = selectedProcess }
    function identity(row) { return String(row.pid) + ":" + String(row.startTimeTicks) }
    function metric(value, suffix) { return isFinite(Number(value)) ? Number(value).toFixed(1) + suffix : "Unavailable" }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        // Search, Sort & Summary Control Strip
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: controlsRow.implicitHeight + Kirigami.Units.smallSpacing * 2
            radius: theme.innerRadius
            color: theme.surfaceColor
            border.width: 1
            border.color: theme.cardBorderColor

            RowLayout {
                id: controlsRow
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "system-search"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: Kirigami.Theme.disabledTextColor
                }

                QQC2.TextField {
                    id: search
                    objectName: "processSearch"
                    Layout.fillWidth: true
                    placeholderText: "Filter processes by name, PID or owner…"
                    Accessible.name: "Search processes"
                }

                QQC2.Label {
                    text: "Sort:"
                    color: Kirigami.Theme.disabledTextColor
                    font.bold: true
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }

                QQC2.ComboBox {
                    id: sort
                    objectName: "processSort"
                    model: ["CPU use", "Memory use", "Name"]
                    Accessible.name: "Sort processes"
                }

                Rectangle {
                    radius: theme.pillRadius
                    color: theme.subtleSurfaceColor
                    border.width: 1
                    border.color: theme.subtleBorderColor
                    implicitWidth: countLbl.implicitWidth + 14
                    implicitHeight: countLbl.implicitHeight + 6

                    QQC2.Label {
                        id: countLbl
                        anchors.centerIn: parent
                        text: page.visibleProcesses.length + " processes"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        font.bold: true
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }

        // Table Header
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: headerRow.implicitHeight + 8
            color: theme.tableHeaderBg
            radius: theme.pillRadius
            border.width: 1
            border.color: theme.subtleBorderColor

            RowLayout {
                id: headerRow
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                QQC2.Label {
                    Layout.fillWidth: true
                    text: "PROCESS"
                    font.bold: true
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    Layout.preferredWidth: 64
                    horizontalAlignment: Text.AlignRight
                    text: "PID"
                    font.bold: true
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    visible: page.width >= 620
                    Layout.preferredWidth: 80
                    text: "USER"
                    font.bold: true
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    Layout.preferredWidth: 100
                    horizontalAlignment: Text.AlignRight
                    text: "CPU %"
                    font.bold: true
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    Layout.preferredWidth: 90
                    horizontalAlignment: Text.AlignRight
                    text: "RESIDENT RAM"
                    font.bold: true
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    visible: page.width >= 560
                    Layout.preferredWidth: 44
                    horizontalAlignment: Text.AlignHCenter
                    text: "STATE"
                    font.bold: true
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        // Process List View
        ListView {
            id: processList
            objectName: "processList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: page.visibleProcesses
            spacing: 2
            reuseItems: true
            QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

            delegate: QQC2.ItemDelegate {
                id: processRow
                required property var modelData
                required property int index
                objectName: "processRow-" + page.identity(modelData)
                width: processList.width
                highlighted: page.selectedIdentity === page.identity(modelData)
                text: modelData.name + " · PID " + modelData.pid
                      + " · " + (modelData.cpuAvailable === false ? "CPU warming up" : page.metric(modelData.cpuPercent, "% CPU"))
                      + " · " + page.metric(modelData.rssMegabytes, " MiB")
                Accessible.name: text
                onClicked: { page.selectedIdentity = page.identity(modelData); page.lastSelection = modelData }

                background: Rectangle {
                    radius: theme.innerRadius
                    color: processRow.highlighted
                           ? theme.accentTint(Kirigami.Theme.highlightColor, theme.isDark ? 0.22 : 0.15)
                           : (processRow.hovered ? theme.tableRowHoverBg : (processRow.index % 2 === 1 ? theme.tableRowAltBg : "transparent"))
                    border.width: processRow.highlighted ? 1 : 0
                    border.color: Kirigami.Theme.highlightColor
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8

                    // Process Icon + Name
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Kirigami.Icon {
                            source: processRow.modelData.owner === "root" ? "system-run" : "application-x-executable"
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                            color: processRow.highlighted ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: processRow.modelData.name
                            font.bold: processRow.highlighted || (processRow.modelData.cpuPercent > 10)
                            color: Kirigami.Theme.textColor
                            elide: Text.ElideRight
                        }
                    }

                    // PID Column
                    QQC2.Label {
                        Layout.preferredWidth: 64
                        horizontalAlignment: Text.AlignRight
                        text: processRow.modelData.pid
                        font.family: "monospace"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                    }

                    // User / Owner Column
                    QQC2.Label {
                        visible: page.width >= 620
                        Layout.preferredWidth: 80
                        text: processRow.modelData.owner || "—"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }

                    // CPU % Column with mini bar
                    RowLayout {
                        Layout.preferredWidth: 100
                        spacing: 6

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 4
                            radius: 2
                            color: theme.trackColor
                            Rectangle {
                                width: parent.width * Math.min(1.0, Math.max(0.0, (processRow.modelData.cpuPercent || 0) / 100.0))
                                height: parent.height
                                radius: 2
                                color: (processRow.modelData.cpuPercent || 0) > 50
                                       ? Kirigami.Theme.negativeTextColor
                                       : ((processRow.modelData.cpuPercent || 0) > 20 ? Kirigami.Theme.neutralTextColor : theme.cpuAccent)
                            }
                        }

                        QQC2.Label {
                            Layout.preferredWidth: 48
                            horizontalAlignment: Text.AlignRight
                            text: processRow.modelData.cpuAvailable === false
                                  ? "wait…" : page.metric(processRow.modelData.cpuPercent, "%")
                            font.bold: (processRow.modelData.cpuPercent || 0) > 10
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: (processRow.modelData.cpuPercent || 0) > 50
                                   ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                        }
                    }

                    // Memory RSS Column
                    QQC2.Label {
                        Layout.preferredWidth: 90
                        horizontalAlignment: Text.AlignRight
                        text: page.metric(processRow.modelData.rssMegabytes, " MiB")
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        color: Kirigami.Theme.textColor
                    }

                    // State Badge Column
                    Rectangle {
                        visible: page.width >= 560
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 18
                        radius: 4
                        color: processRow.modelData.inDiskWait
                               ? theme.accentTint(Kirigami.Theme.negativeTextColor, 0.20)
                               : (processRow.modelData.state === "R"
                                  ? theme.accentTint(Kirigami.Theme.positiveTextColor, 0.16)
                                  : theme.subtleSurfaceColor)
                        border.width: 1
                        border.color: processRow.modelData.inDiskWait
                                      ? Kirigami.Theme.negativeTextColor
                                      : (processRow.modelData.state === "R" ? Kirigami.Theme.positiveTextColor : theme.subtleBorderColor)

                        QQC2.Label {
                            anchors.centerIn: parent
                            text: processRow.modelData.inDiskWait ? "D (wait)" : (processRow.modelData.state || "—")
                            font.bold: true
                            font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                            color: processRow.modelData.inDiskWait
                                   ? Kirigami.Theme.negativeTextColor
                                   : (processRow.modelData.state === "R" ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor)
                        }
                    }
                }
            }

            QQC2.Label {
                anchors.centerIn: parent
                visible: processList.count === 0
                text: search.text.length ? "No matching processes found" : "Waiting for readable process observations…"
                color: Kirigami.Theme.disabledTextColor
                font.bold: true
            }
        }

        // Selected Process Inspector Card
        Rectangle {
            Layout.fillWidth: true
            visible: page.selectedIdentity.length > 0
            implicitHeight: selCardCol.implicitHeight + Kirigami.Units.mediumSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1
            border.color: theme.cardBorderColor

            ColumnLayout {
                id: selCardCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.mediumSpacing
                spacing: 6
                readonly property var row: page.selectedProcess || page.lastSelection || ({})

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        radius: 5
                        color: theme.accentTint(Kirigami.Theme.highlightColor, 0.18)
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: "dialog-information"
                            color: Kirigami.Theme.highlightColor
                        }
                    }

                    Kirigami.Heading {
                        level: 4
                        text: (selCardCol.row.name || "Process") + " (PID " + (selCardCol.row.pid || "") + ")"
                        color: Kirigami.Theme.textColor
                        Layout.fillWidth: true
                    }

                    PulseButton {
                        text: "System Monitor"
                        icon.name: "utilities-system-monitor"
                        onClicked: page.controller.openSystemMonitor()
                    }

                    QQC2.Label {
                        text: page.selectedProcess ? "Active observation" : "Terminated / No longer observed"
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        font.bold: true
                        color: page.selectedProcess ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6

                    Rectangle {
                        radius: theme.pillRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        implicitWidth: p1.implicitWidth + 12
                        implicitHeight: p1.implicitHeight + 4
                        QQC2.Label { id: p1; anchors.centerIn: parent; text: "Owner: " + (selCardCol.row.owner || "Unavailable"); font.pixelSize: Kirigami.Theme.smallFont.pixelSize }
                    }
                    Rectangle {
                        radius: theme.pillRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        implicitWidth: p2.implicitWidth + 12
                        implicitHeight: p2.implicitHeight + 4
                        QQC2.Label { id: p2; anchors.centerIn: parent; text: "State: " + (selCardCol.row.state || "Unavailable"); font.pixelSize: Kirigami.Theme.smallFont.pixelSize }
                    }
                    Rectangle {
                        radius: theme.pillRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        implicitWidth: p3.implicitWidth + 12
                        implicitHeight: p3.implicitHeight + 4
                        QQC2.Label {
                            id: p3
                            anchors.centerIn: parent
                            text: "CPU: " + (selCardCol.row.cpuAvailable === false ? "Warming up" : page.metric(selCardCol.row.cpuPercent, "%"))
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }
                    Rectangle {
                        radius: theme.pillRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        implicitWidth: p4.implicitWidth + 12
                        implicitHeight: p4.implicitHeight + 4
                        QQC2.Label {
                            id: p4
                            anchors.centerIn: parent
                            text: "RSS: " + page.metric(selCardCol.row.rssMegabytes, " MiB")
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }
                    Rectangle {
                        radius: theme.pillRadius
                        color: theme.subtleSurfaceColor
                        border.width: 1
                        border.color: theme.subtleBorderColor
                        implicitWidth: p5.implicitWidth + 12
                        implicitHeight: p5.implicitHeight + 4
                        QQC2.Label {
                            id: p5
                            anchors.centerIn: parent
                            text: "Observed: " + (Number(selCardCol.row.observedAtMs) > 0 ? new Date(Number(selCardCol.row.observedAtMs)).toLocaleTimeString() : "—")
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 1)
                    color: Kirigami.Theme.disabledTextColor
                    text: "Observed CPU and resident memory describe resource use; a busy process alone does not establish causation."
                }
            }
        }
    }
}
