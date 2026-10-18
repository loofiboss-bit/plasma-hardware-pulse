pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    objectName: "processesPage"
    title: "Processes"
    required property var controller
    property string selectedIdentity: ""
    property var lastSelection: null
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
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Live process observations · updated every 2 seconds. CPU and resident memory describe observed use; a busy process alone does not establish a bottleneck."
        }
        RowLayout {
            Layout.fillWidth: true
            QQC2.TextField { id: search; objectName: "processSearch"; Layout.fillWidth: true; placeholderText: "Search name, PID or owner"; Accessible.name: "Search processes" }
            QQC2.ComboBox { id: sort; objectName: "processSort"; model: ["CPU use", "Memory use", "Name"]; Accessible.name: "Sort processes" }
        }
        QQC2.Label { text: page.visibleProcesses.length + " readable processes" }
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
                objectName: "processRow-" + page.identity(modelData)
                width: processList.width
                highlighted: page.selectedIdentity === page.identity(modelData)
                text: modelData.name + " · PID " + modelData.pid
                      + " · " + (modelData.cpuAvailable === false ? "CPU warming up" : page.metric(modelData.cpuPercent, "% CPU"))
                      + " · " + page.metric(modelData.rssMegabytes, " MiB")
                Accessible.name: text
                onClicked: { page.selectedIdentity = page.identity(modelData); page.lastSelection = modelData }
            }
            QQC2.Label {
                anchors.centerIn: parent
                visible: processList.count === 0
                text: search.text.length ? "No matching processes" : "Waiting for readable process observations"
            }
        }
        QQC2.GroupBox {
            title: "Selected process"
            Layout.fillWidth: true
            visible: page.selectedIdentity.length > 0
            ColumnLayout {
                width: parent.width
                readonly property var row: page.selectedProcess || page.lastSelection || ({})
                QQC2.Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: parent.row.name + " · PID " + parent.row.pid + " · Owner " + (parent.row.owner || "Unavailable")
                          + " · State " + (parent.row.state || "Unavailable")
                }
                QQC2.Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: (page.selectedProcess ? "Last observed " : "No longer observed. Last selection: ")
                          + new Date(Number(parent.row.observedAtMs)).toLocaleTimeString()
                          + " · " + (parent.row.cpuAvailable === false ? "CPU warming up" : page.metric(parent.row.cpuPercent, "% CPU"))
                          + " · " + page.metric(parent.row.rssMegabytes, " MiB resident memory")
                }
            }
        }
    }
}
