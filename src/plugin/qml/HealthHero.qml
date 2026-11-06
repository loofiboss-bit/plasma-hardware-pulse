// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami

// Overview Hero Banner: 6-domain health ring gauge, headline diagnosis with
// evidence & uncertainty chips, and a right-hand recent-activity summary.
PulseCard {
    id: hero

    property var controller: null
    property var domainSeverities: [0, 0, 0, 0, 0, 0] // CPU, Temp, GPU, Mem, Storage, Power

    signal markIncidentClicked()
    signal saveInvestigationClicked()

    readonly property int okDomainCount: {
        var count = 0
        for (var i = 0; i < domainSeverities.length; ++i) {
            if (domainSeverities[i] === 0) count++
        }
        return count
    }

    readonly property var domainSegmentColors: {
        var cols = []
        for (var i = 0; i < domainSeverities.length; ++i) {
            cols.push(theme.severityColor(domainSeverities[i]))
        }
        return cols
    }

    accentColor: controller ? theme.severityColor(controller.verdictSeverity) : theme.positiveColor
    accentStrength: controller && controller.verdictSeverity > 0 ? (theme.isDark ? 0.14 : 0.08) : 0
    padding: theme.spacingL

    implicitWidth: layout.implicitWidth + padding * 2
    implicitHeight: layout.implicitHeight + padding * 2

    GridLayout {
        id: layout
        anchors.fill: parent
        columns: hero.width < 800 ? 1 : (hero.width < 1100 ? 2 : 3)
        rowSpacing: hero.theme.spacingM
        columnSpacing: hero.theme.spacingL

        // Column 1: Honest 6-domain health ring
        Item {
            Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
            Layout.preferredWidth: Kirigami.Units.gridUnit * 7
            Layout.preferredHeight: Layout.preferredWidth

            PulseRingGauge {
                id: ring
                anchors.centerIn: parent
                width: parent.width
                height: parent.height
                theme: hero.theme
                segments: hero.domainSegmentColors
                pulsing: hero.controller ? hero.controller.verdictSeverity >= 2 : false
                thickness: 9

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 0

                    QQC2.Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: hero.okDomainCount + "/" + hero.domainSeverities.length
                        font.pointSize: hero.theme.fontHeadline
                        font.weight: Font.Bold
                        font.features: { "tnum": 1 }
                        color: hero.theme.textColor
                    }

                    QQC2.Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: KI18n.i18nd("loofi-pulse", "DOMAINS OK")
                        font.pointSize: hero.theme.fontCaption * 0.85
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.5
                        color: hero.theme.mutedTextColor
                    }
                }
            }
        }

        // Column 2: Diagnostic verdict & evidence chips
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: hero.theme.spacingS

            RowLayout {
                spacing: hero.theme.spacingS
                StatusPill {
                    theme: hero.theme
                    severity: hero.controller && hero.controller.verdictSeverity !== undefined ? hero.controller.verdictSeverity : 0
                    text: String((hero.controller && hero.controller.verdictBadgeText) || "")
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: String((hero.controller && hero.controller.verdictHeadline) || KI18n.i18nd("loofi-pulse", "Hardware Telemetry Initializing"))
                font.pointSize: hero.theme.fontTitle
                font.weight: Font.DemiBold
                color: hero.theme.textColor
                wrapMode: Text.WordWrap
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: String((hero.controller && hero.controller.verdictExplanation) || "")
                font.pointSize: hero.theme.fontBody
                color: hero.theme.mutedTextColor
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }

            // Evidence & Uncertainty Chips
            Flow {
                Layout.fillWidth: true
                spacing: 6
                visible: hero.controller && hero.controller.verdictSeverity > 0

                Rectangle {
                    radius: 999
                    color: hero.theme.subtleSurfaceColor
                    border.width: 1
                    border.color: hero.theme.cardBorderColor
                    implicitHeight: chipEvLabel.implicitHeight + 6
                    implicitWidth: chipEvLabel.implicitWidth + 16

                    RowLayout {
                        id: chipEvLabel
                        anchors.centerIn: parent
                        spacing: 4
                        QQC2.Label {
                            text: KI18n.i18nd("loofi-pulse", "Evidence")
                            font.pointSize: hero.theme.fontCaption
                            color: hero.theme.mutedTextColor
                        }
                        QQC2.Label {
                            text: String(hero.controller && hero.controller.verdictEvidenceCount !== undefined ? hero.controller.verdictEvidenceCount : "0")
                            font.pointSize: hero.theme.fontCaption
                            font.weight: Font.Bold
                            color: hero.theme.textColor
                        }
                    }
                }

                Rectangle {
                    radius: 999
                    color: hero.theme.subtleSurfaceColor
                    border.width: 1
                    border.color: hero.theme.cardBorderColor
                    implicitHeight: chipMissLabel.implicitHeight + 6
                    implicitWidth: chipMissLabel.implicitWidth + 16

                    RowLayout {
                        id: chipMissLabel
                        anchors.centerIn: parent
                        spacing: 4
                        QQC2.Label {
                            text: KI18n.i18nd("loofi-pulse", "Missing")
                            font.pointSize: hero.theme.fontCaption
                            color: hero.theme.mutedTextColor
                        }
                        QQC2.Label {
                            text: String(hero.controller && hero.controller.verdictMissingEvidenceCount !== undefined ? hero.controller.verdictMissingEvidenceCount : "0")
                            font.pointSize: hero.theme.fontCaption
                            font.weight: Font.Bold
                            color: hero.theme.textColor
                        }
                    }
                }

                Rectangle {
                    radius: 999
                    color: hero.theme.subtleSurfaceColor
                    border.width: 1
                    border.color: hero.theme.cardBorderColor
                    implicitHeight: chipFreshLabel.implicitHeight + 6
                    implicitWidth: chipFreshLabel.implicitWidth + 16

                    RowLayout {
                        id: chipFreshLabel
                        anchors.centerIn: parent
                        spacing: 4
                        QQC2.Label {
                            text: KI18n.i18nd("loofi-pulse", "Freshness")
                            font.pointSize: hero.theme.fontCaption
                            color: hero.theme.mutedTextColor
                        }
                        QQC2.Label {
                            text: hero.controller && hero.controller.verdictFreshnessSeconds !== undefined ? (hero.controller.verdictFreshnessSeconds + " s") : "0 s"
                            font.pointSize: hero.theme.fontCaption
                            font.weight: Font.Bold
                            color: hero.theme.textColor
                        }
                    }
                }

                Rectangle {
                    radius: 999
                    color: hero.theme.subtleSurfaceColor
                    border.width: 1
                    border.color: hero.theme.cardBorderColor
                    implicitHeight: chipUncertLabel.implicitHeight + 6
                    implicitWidth: chipUncertLabel.implicitWidth + 16

                    RowLayout {
                        id: chipUncertLabel
                        anchors.centerIn: parent
                        spacing: 4
                        QQC2.Label {
                            text: KI18n.i18nd("loofi-pulse", "Confidence")
                            font.pointSize: hero.theme.fontCaption
                            color: hero.theme.mutedTextColor
                        }
                        QQC2.Label {
                            text: String((hero.controller && hero.controller.verdictUncertaintyState) || "")
                            font.pointSize: hero.theme.fontCaption
                            font.weight: Font.Bold
                            color: hero.theme.textColor
                        }
                    }
                }
            }
        }

        // Column 3: Recent Activity, Incidents & Power Profile
        ColumnLayout {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: hero.width < 1100 ? -1 : Kirigami.Units.gridUnit * 13
            Layout.fillWidth: hero.width < 1100
            spacing: hero.theme.spacingS

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: hero.theme.subtleBorderColor
                visible: hero.width < 1100
            }

            QQC2.Label {
                text: KI18n.i18nd("loofi-pulse", "RECENT ACTIVITY (15M)")
                font.pointSize: hero.theme.fontCaption * 0.9
                font.weight: Font.DemiBold
                font.letterSpacing: 0.6
                color: hero.theme.faintTextColor
            }

            PulseSparkline {
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 3
                color: hero.controller && hero.controller.verdictSeverity > 0 ? hero.theme.severityColor(hero.controller.verdictSeverity) : hero.theme.cpuAccent
                values: hero.controller ? JSON.parse(hero.controller.historyRecentSeriesJson("cpu.load", "cpu.load.avail", 30)) : []
            }

            RowLayout {
                Layout.fillWidth: true
                QQC2.Label {
                    Layout.fillWidth: true
                    text: KI18n.i18nd("loofi-pulse", "Incidents")
                    font.pointSize: hero.theme.fontBody
                    color: hero.theme.mutedTextColor
                }
                QQC2.Label {
                    text: hero.controller && hero.controller.recentIncidentsCount !== undefined
                          ? KI18n.i18ndp("loofi-pulse", "%1 recorded", "%1 recorded", hero.controller.recentIncidentsCount)
                          : "0"
                    font.pointSize: hero.theme.fontBody
                    font.weight: Font.Medium
                    color: hero.theme.textColor
                }
            }

            RowLayout {
                Layout.fillWidth: true
                QQC2.Label {
                    Layout.fillWidth: true
                    text: KI18n.i18nd("loofi-pulse", "Power profile")
                    font.pointSize: hero.theme.fontBody
                    color: hero.theme.mutedTextColor
                }
                QQC2.Label {
                    text: String((hero.controller && hero.controller.activePowerProfile) || KI18n.i18nd("loofi-pulse", "Balanced"))
                    font.pointSize: hero.theme.fontBody
                    font.weight: Font.Medium
                    color: hero.theme.textColor
                }
            }
        }
    }
}
