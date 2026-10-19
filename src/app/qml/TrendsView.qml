pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: root

    required property var controller
    required property var theme
    signal openSettingsRequested()

    radius: theme.cardRadius
    color: theme.surfaceColor
    border.width: 1
    border.color: theme.cardBorderColor
    implicitHeight: mainLayout.implicitHeight + Kirigami.Units.largeSpacing * 2

    readonly property bool isHistoryEnabled: Boolean(controller.collectorPreferences && controller.collectorPreferences.longTermHistoryEnabled)
    readonly property var trends: controller.trendsData || ({})
    readonly property var series: trends.series || ({})
    readonly property var gaps: trends.gaps || []
    readonly property double coverageMs: Number(trends.coverageMs || 0)
    readonly property double totalSpanMs: {
        var range = controller.trendsRange || "1h"
        if (range === "6h") return 6 * 3600000
        if (range === "24h") return 24 * 3600000
        if (range === "7d") return 7 * 86400000
        if (range === "30d") return 30 * 86400000
        return 3600000
    }
    readonly property double coveragePercent: totalSpanMs > 0 ? Math.min(100.0, (coverageMs / totalSpanMs) * 100.0) : 0.0

    Component.onCompleted: {
        if (root.isHistoryEnabled) {
            root.controller.loadTrendsRange(root.controller.trendsRange || "1h")
        }
    }

    Connections {
        target: root.controller
        function onCollectorPreferencesChanged() {
            if (root.isHistoryEnabled && (!root.controller.trendsData || Object.keys(root.controller.trendsData).length === 0)) {
                root.controller.loadTrendsRange(root.controller.trendsRange || "1h")
            }
        }
    }

    ColumnLayout {
        id: mainLayout
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        // --- Header & Range Switcher ---
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.mediumSpacing

            Kirigami.Heading {
                level: 3
                text: "Long-Term Trends & Incident History"
                Layout.fillWidth: true
            }

            // Range buttons
            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                visible: root.isHistoryEnabled

                Repeater {
                    model: [
                        { label: "1h", key: "1h" },
                        { label: "6h", key: "6h" },
                        { label: "24h", key: "24h" },
                        { label: "7d", key: "7d" },
                        { label: "30d", key: "30d" }
                    ]

                    delegate: PulseButton {
                        required property var modelData
                        text: modelData.label
                        checkable: true
                        checked: root.controller.trendsRange === modelData.key
                        onClicked: root.controller.loadTrendsRange(modelData.key)
                    }
                }

                PulseButton {
                    icon.name: "view-refresh"
                    text: "Refresh"
                    enabled: !root.controller.trendsLoading
                    onClicked: root.controller.loadTrendsRange(root.controller.trendsRange || "1h")
                }
            }
        }

        // --- Disabled State Banner ---
        Rectangle {
            id: disabledBanner
            Layout.fillWidth: true
            implicitHeight: disabledCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            visible: !root.isHistoryEnabled
            radius: root.theme.cardRadius
            color: Qt.rgba(root.theme.neutralColor.r, root.theme.neutralColor.g, root.theme.neutralColor.b, 0.08)
            border.width: 1
            border.color: Qt.rgba(root.theme.neutralColor.r, root.theme.neutralColor.g, root.theme.neutralColor.b, 0.2)

            ColumnLayout {
                id: disabledCol
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                RowLayout {
                    spacing: Kirigami.Units.mediumSpacing

                    Kirigami.Icon {
                        source: "view-history"
                        implicitWidth: 32
                        implicitHeight: 32
                        color: root.theme.neutralColor
                    }

                    ColumnLayout {
                        spacing: 2
                        QQC2.Label {
                            text: "Long-Term Metrics History is Disabled"
                            font.bold: true
                            font.pointSize: 12
                        }
                        QQC2.Label {
                            text: "Enable long-term history to record 1-minute system hardware trends across restarts (up to 30 days)."
                            color: root.theme.disabledColor
                        }
                    }

                    Item { Layout.fillWidth: true }

                    PulseButton {
                        text: "Enable in Settings…"
                        icon.name: "configure"
                        isPrimary: true
                        onClicked: root.openSettingsRequested()
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "🔒 Privacy: Persistent history is stored in a private SQLite database with strict 64 MB storage limits. Only numerical hardware aggregates (CPU, thermals, GPU, RAM, disk throughput) are recorded. Process names, commands, window titles, and user PIDs are never stored."
                    font.pointSize: 9
                    color: root.theme.disabledColor
                }
            }
        }

        // --- Enabled State: Trends Content ---
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing
            visible: root.isHistoryEnabled

            // 1. Overview Metric Chips
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.mediumSpacing

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 72
                    radius: root.theme.cardRadius
                    color: Qt.rgba(root.theme.positiveColor.r, root.theme.positiveColor.g, root.theme.positiveColor.b, 0.08)
                    border.width: 1
                    border.color: Qt.rgba(root.theme.positiveColor.r, root.theme.positiveColor.g, root.theme.positiveColor.b, 0.2)

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2
                        QQC2.Label {
                            text: "MONITORING COVERAGE"
                            font.bold: true
                            font.pointSize: 8
                            color: root.theme.disabledColor
                        }
                        QQC2.Label {
                            text: root.coveragePercent.toFixed(1) + "%"
                            font.bold: true
                            font.pointSize: 15
                            color: root.coveragePercent > 80 ? root.theme.positiveColor : root.theme.neutralColor
                        }
                        QQC2.Label {
                            text: (root.coverageMs / 3600000).toFixed(1) + " h recorded"
                            font.pointSize: 8
                            color: root.theme.disabledColor
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 72
                    radius: root.theme.cardRadius
                    color: Qt.rgba(root.theme.neutralColor.r, root.theme.neutralColor.g, root.theme.neutralColor.b, 0.08)
                    border.width: 1
                    border.color: Qt.rgba(root.theme.neutralColor.r, root.theme.neutralColor.g, root.theme.neutralColor.b, 0.2)

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2
                        QQC2.Label {
                            text: "DATA POINTS"
                            font.bold: true
                            font.pointSize: 8
                            color: root.theme.disabledColor
                        }
                        QQC2.Label {
                            property var cpuList: root.series.cpuLoad || []
                            text: String(cpuList.length) + " buckets"
                            font.bold: true
                            font.pointSize: 15
                        }
                        QQC2.Label {
                            text: "1-minute resolution"
                            font.pointSize: 8
                            color: root.theme.disabledColor
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 72
                    radius: root.theme.cardRadius
                    color: Qt.rgba(root.theme.negativeColor.r, root.theme.negativeColor.g, root.theme.negativeColor.b, 0.08)
                    border.width: 1
                    border.color: Qt.rgba(root.theme.negativeColor.r, root.theme.negativeColor.g, root.theme.negativeColor.b, 0.2)

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2
                        QQC2.Label {
                            text: "RECORDED GAPS"
                            font.bold: true
                            font.pointSize: 8
                            color: root.theme.disabledColor
                        }
                        QQC2.Label {
                            text: String(root.gaps.length)
                            font.bold: true
                            font.pointSize: 15
                            color: root.gaps.length > 0 ? root.theme.neutralColor : root.theme.positiveColor
                        }
                        QQC2.Label {
                            text: root.gaps.length > 0 ? "Suspend/Offline" : "Continuous"
                            font.pointSize: 8
                            color: root.theme.disabledColor
                        }
                    }
                }
            }

            // 2. Trend Canvas Plot: CPU Load & CPU Package Temp
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 220
                radius: root.theme.cardRadius
                color: root.Kirigami.Theme.backgroundColor
                border.width: 1
                border.color: root.theme.cardBorderColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        QQC2.Label {
                            text: "CPU Load & Package Temperature Trends"
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        QQC2.Label {
                            text: "■ CPU Load (%)  ■ CPU Temp (" + (root.controller.temperatureUnit === "F" ? "°F" : "°C") + ")"
                            font.pointSize: 9
                            color: root.theme.disabledColor
                        }
                    }

                    Canvas {
                        id: trendCanvas
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Connections {
                            target: root.controller
                            function onTrendsDataChanged() { trendCanvas.requestPaint() }
                        }

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)

                            var cpuData = root.series.cpuLoad || []
                            var tempData = root.series.cpuTemp || []
                            if (cpuData.length === 0) {
                                ctx.fillStyle = root.theme.disabledColor
                                ctx.font = "12px sans-serif"
                                ctx.textAlign = "center"
                                ctx.fillText("No persisted history samples for this time window yet.", width / 2, height / 2)
                                return
                            }

                            var pad = 30
                            var plotW = width - pad * 2
                            var plotH = height - 20

                            // Grid lines
                            ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.08)
                            ctx.lineWidth = 1
                            for (var g = 0; g <= 4; ++g) {
                                var y = 10 + (plotH / 4) * g
                                ctx.beginPath()
                                ctx.moveTo(pad, y)
                                ctx.lineTo(pad + plotW, y)
                                ctx.stroke()
                            }

                            // Plot CPU Load (Blue-cyan fill and line)
                            var step = plotW / Math.max(1, cpuData.length - 1)
                            ctx.beginPath()
                            ctx.moveTo(pad, 10 + plotH)
                            for (var i = 0; i < cpuData.length; ++i) {
                                var avg = cpuData[i].avg || 0
                                var px = pad + i * step
                                var py = 10 + plotH - (avg / 100.0) * plotH
                                ctx.lineTo(px, py)
                            }
                            ctx.lineTo(pad + (cpuData.length - 1) * step, 10 + plotH)
                            ctx.closePath()
                            ctx.fillStyle = Qt.rgba(0.2, 0.6, 1.0, 0.15)
                            ctx.fill()

                            // CPU Load Line
                            ctx.beginPath()
                            for (var j = 0; j < cpuData.length; ++j) {
                                var cAvg = cpuData[j].avg || 0
                                var cX = pad + j * step
                                var cY = 10 + plotH - (cAvg / 100.0) * plotH
                                if (j === 0) ctx.moveTo(cX, cY)
                                else ctx.lineTo(cX, cY)
                            }
                            ctx.strokeStyle = Qt.rgba(0.2, 0.7, 1.0, 0.85)
                            ctx.lineWidth = 2
                            ctx.stroke()

                            // Plot CPU Temp Line (Orange/Red)
                            if (tempData.length > 0) {
                                var tStep = plotW / Math.max(1, tempData.length - 1)
                                ctx.beginPath()
                                for (var k = 0; k < tempData.length; ++k) {
                                    var tAvg = tempData[k].avg || 0
                                    // Scale 20°C - 100°C
                                    var tNorm = Math.max(0, Math.min(1.0, (tAvg - 20.0) / 80.0))
                                    var tX = pad + k * tStep
                                    var tY = 10 + plotH - tNorm * plotH
                                    if (k === 0) ctx.moveTo(tX, tY)
                                    else ctx.lineTo(tX, tY)
                                }
                                ctx.strokeStyle = Qt.rgba(1.0, 0.45, 0.15, 0.85)
                                ctx.lineWidth = 2
                                ctx.stroke()
                            }
                        }
                    }
                }
            }

            // 3. Persistent Incidents Log Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: incCol.implicitHeight + Kirigami.Units.mediumSpacing * 2
                radius: root.theme.cardRadius
                color: root.Kirigami.Theme.backgroundColor
                border.width: 1
                border.color: root.theme.cardBorderColor

                ColumnLayout {
                    id: incCol
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        QQC2.Label {
                            text: "Recorded Incidents & Hardware Events"
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        QQC2.Label {
                            text: (root.controller.persistedIncidents || []).length + " incidents recorded"
                            font.pointSize: 9
                            color: root.theme.disabledColor
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: (root.controller.persistedIncidents || []).length === 0
                        text: "No severe incidents or thermal throttling events occurred during this time period."
                        color: root.theme.disabledColor
                        font.pointSize: 9
                    }

                    Repeater {
                        model: root.controller.persistedIncidents || []

                        delegate: Rectangle {
                            id: incDelegate
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 44
                            radius: 4
                            color: incDelegate.modelData.severity >= 3 ? Qt.rgba(root.theme.negativeColor.r, root.theme.negativeColor.g, root.theme.negativeColor.b, 0.1) : Qt.rgba(1, 1, 1, 0.03)

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Kirigami.Units.mediumSpacing
                                anchors.rightMargin: Kirigami.Units.mediumSpacing
                                spacing: Kirigami.Units.mediumSpacing

                                Kirigami.Icon {
                                    source: incDelegate.modelData.severity >= 3 ? "dialog-error" : "dialog-warning"
                                    implicitWidth: 16
                                    implicitHeight: 16
                                    color: incDelegate.modelData.severity >= 3 ? root.theme.negativeColor : root.theme.neutralColor
                                }

                                ColumnLayout {
                                    spacing: 1
                                    QQC2.Label {
                                        text: incDelegate.modelData.headline || "Incident"
                                        font.bold: true
                                        font.pointSize: 9
                                    }
                                    QQC2.Label {
                                        text: "Domain: " + (incDelegate.modelData.domain || "system") + " · Rule: " + (incDelegate.modelData.ruleId || "auto")
                                        font.pointSize: 8
                                        color: root.theme.disabledColor
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                QQC2.Label {
                                    text: Qt.formatDateTime(new Date(Number(incDelegate.modelData.startMs || 0)), "yyyy-MM-dd HH:mm")
                                    font.pointSize: 8
                                    color: root.theme.disabledColor
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
