pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: root
    objectName: "trendsView"

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
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.mediumSpacing

            Kirigami.Heading {
                level: 3
                text: KI18n.i18n("Long-Term Trends & Incident History")
                Layout.fillWidth: true
            }

            // Range buttons
            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                visible: root.isHistoryEnabled

                Repeater {
                    model: [
                        { label: KI18n.i18n("1h"), key: "1h" },
                        { label: KI18n.i18n("6h"), key: "6h" },
                        { label: KI18n.i18n("24h"), key: "24h" },
                        { label: KI18n.i18n("7d"), key: "7d" },
                        { label: KI18n.i18n("30d"), key: "30d" }
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
                    text: KI18n.i18n("Refresh")
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

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.mediumSpacing

                    Kirigami.Icon {
                        source: "view-history"
                        implicitWidth: 32
                        implicitHeight: 32
                        color: root.theme.neutralColor
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: KI18n.i18n("Long-Term Metrics History is Disabled")
                            font.bold: true
                            font.pointSize: 12
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: KI18n.i18n("Enable long-term history to record 1-minute system hardware trends across restarts (up to 30 days).")
                            color: root.theme.disabledColor
                        }
                    }

                    Item { Layout.fillWidth: true }

                    PulseButton {
                        text: KI18n.i18n("Enable in Settings…")
                        icon.name: "configure"
                        isPrimary: true
                        onClicked: root.openSettingsRequested()
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: KI18n.i18n("🔒 Privacy: Persistent history is stored in a private SQLite database with strict 64 MB storage limits. Only numerical hardware aggregates (CPU, thermals, GPU, RAM, disk throughput) are recorded. Process names, commands, window titles, and user PIDs are never stored.")
                    font.pointSize: 9
                    color: root.theme.disabledColor
                }
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.controller.trendsLoading || root.controller.persistedIncidentsLoading
            text: KI18n.i18n("Loading history…")
            type: Kirigami.MessageType.Information
        }
        Kirigami.InlineMessage {
            objectName: "trendsErrorBanner"
            Layout.fillWidth: true
            visible: Boolean(root.controller.trendsError || root.controller.persistedIncidentsError || root.trends.storageError)
            text: root.controller.trendsError || root.controller.persistedIncidentsError || root.trends.storageError || ""
            type: Kirigami.MessageType.Error
            actions: [Kirigami.Action { text: KI18n.i18n("Retry"); onTriggered: root.controller.loadTrendsRange(root.controller.trendsRange || "1h") }]
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: Number(root.trends.calendarTimeUnknownIncidents || 0) > 0
            text: KI18n.i18n("Older incident summaries have no calendar timestamp and cannot be placed in this time range.")
            type: Kirigami.MessageType.Warning
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
                            text: KI18n.i18n("MONITORING COVERAGE")
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
                            text: (root.coverageMs / 3600000).toFixed(1) + KI18n.i18n(" h recorded")
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
                            text: KI18n.i18n("DATA POINTS")
                            font.bold: true
                            font.pointSize: 8
                            color: root.theme.disabledColor
                        }
                        QQC2.Label {
                            property var cpuList: root.series.cpuLoad || []
                            text: String(cpuList.length) + KI18n.i18n(" buckets")
                            font.bold: true
                            font.pointSize: 15
                        }
                        QQC2.Label {
                            text: KI18n.i18n("Bucket duration: ") + Math.round(Number(root.trends.bucketMs || 60000) / 60000) + KI18n.i18n(" min")
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
                            text: KI18n.i18n("RECORDED GAPS")
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
                            text: root.gaps.length > 0 ? "Suspend/Offline" : KI18n.i18n("No recorded gaps")
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

                    ColumnLayout {
                        Layout.fillWidth: true
                        QQC2.Label {
                            text: KI18n.i18n("CPU Load & Package Temperature Trends")
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        QQC2.Label {
                            text: KI18n.i18n("CPU load: 0–100% · Temperature: ") + (root.controller.temperatureUnit === "F" ? KI18n.i18n("68–212 °F") : KI18n.i18n("20–100 °C"))
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

                        Accessible.role: Accessible.Graphic
                        Accessible.name: KI18n.i18n("CPU load and temperature trends")
                        Accessible.description: KI18n.i18n("Time-based chart. Missing observations and offline intervals are gaps. Numerical coverage and bucket duration are displayed above.")
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            const from = Number(root.trends.fromMs || 0)
                            const to = Number(root.trends.toMs || 0)
                            const bucket = Number(root.trends.bucketMs || 60000)
                            const cpu = root.series.cpuLoad || []
                            const temp = root.series.cpuTemp || []
                            if (to <= from || (!cpu.length && !temp.length)) {
                                ctx.fillStyle = root.theme.disabledColor
                                ctx.font = "12px sans-serif"
                                ctx.textAlign = "center"
                                ctx.fillText(KI18n.i18n("No persisted history samples for this time window yet."), width / 2, height / 2)
                                return
                            }
                            const pad = 32
                            const plotW = Math.max(1, width - pad * 2)
                            const plotH = Math.max(1, height - 38)
                            ctx.strokeStyle = root.theme.cardBorderColor
                            ctx.lineWidth = 1
                            for (let g = 0; g <= 4; ++g) {
                                const y = 8 + plotH * g / 4
                                ctx.beginPath(); ctx.moveTo(pad, y); ctx.lineTo(pad + plotW, y); ctx.stroke()
                            }
                            const xAt = function(t) { return pad + (t - from) / (to - from) * plotW }
                            const intersectsGap = function(a, b) {
                                for (let j = 0; j < root.gaps.length; ++j)
                                    if (Number(root.gaps[j].startMs) < b && Number(root.gaps[j].endMs) > a) return true
                                return false
                            }
                            for (let q = 0; q < root.gaps.length; ++q) {
                                const left = xAt(Math.max(from, Number(root.gaps[q].startMs)))
                                const right = xAt(Math.min(to, Number(root.gaps[q].endMs)))
                                ctx.fillStyle = Qt.rgba(root.theme.neutralColor.r, root.theme.neutralColor.g, root.theme.neutralColor.b, 0.15)
                                ctx.fillRect(left, 8, Math.max(0, right - left), plotH)
                            }
                            const drawSeries = function(points, color, temperature) {
                                ctx.beginPath()
                                let previous = null
                                for (let i = 0; i < points.length; ++i) {
                                    const point = points[i]
                                    const time = Math.max(from, Number(point.t))
                                    const value = Number(point.avg)
                                    if (!isFinite(value) || time > to) { previous = null; continue }
                                    const ratio = temperature ? (value - 20) / 80 : value / 100
                                    const x = xAt(time)
                                    const y = 8 + plotH * (1 - Math.max(0, Math.min(1, ratio)))
                                    if (previous === null || time - previous > bucket * 1.5 || intersectsGap(previous, time)) ctx.moveTo(x, y)
                                    else ctx.lineTo(x, y)
                                    ctx.fillStyle = color
                                    ctx.fillRect(x - 1.5, y - 1.5, 3, 3)
                                    previous = time
                                }
                                ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.stroke()
                            }
                            drawSeries(cpu, Qt.rgba(0.2, 0.7, 1.0, 0.85), false)
                            drawSeries(temp, Qt.rgba(1.0, 0.45, 0.15, 0.85), true)
                            ctx.fillStyle = root.theme.disabledColor
                            ctx.font = "10px sans-serif"
                            ctx.textAlign = "left"
                            ctx.fillText(Qt.formatDateTime(new Date(from), "MM-dd HH:mm"), pad, height - 3)
                            ctx.textAlign = "right"
                            ctx.fillText(Qt.formatDateTime(new Date(to), "MM-dd HH:mm"), pad + plotW, height - 3)
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
                            text: KI18n.i18n("Recorded Incidents & Hardware Events")
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        QQC2.Label {
                            text: (root.controller.persistedIncidents || []).length + KI18n.i18n(" incidents recorded")
                            font.pointSize: 9
                            color: root.theme.disabledColor
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: !root.controller.persistedIncidentsLoading && !root.controller.persistedIncidentsError && !root.controller.trendsError && (root.controller.persistedIncidents || []).length === 0
                        text: KI18n.i18n("No incident summaries were recorded in this time range. Missing coverage does not establish that no incidents occurred.")
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
                                        text: KI18n.i18n("Domain: ") + (incDelegate.modelData.domain || "system") + KI18n.i18n(" · Rule: ") + (incDelegate.modelData.ruleId || "auto")
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
