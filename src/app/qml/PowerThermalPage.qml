pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "MeasurementLabels.js" as MeasurementLabels

Kirigami.ScrollablePage {
    id: powerPage
    objectName: "powerPage"
    title: KI18n.i18n("Power & Cooling")
    titleDelegate: Item {
        width: parent ? parent.width : 0
        height: 0
        visible: false
    }
    required property var controller

    function formatTemp(celsius) {
        return MeasurementLabels.formatTemperature(celsius, powerPage.controller.temperatureUnit)
    }

    function focusSection(section) {
        if (!section) return true
        var targets = {"cpu": pkgCol, "gpu": gpuCol, "battery": batCol, "cooling": coolCol, "profile": profCol, "cores": coresHeading, "sensors": sensorsCol}
        var target = targets[section]
        if (!target) return false

        Qt.callLater(function() {
            var position = target.mapToItem(powerPage.flickable.contentItem, 0, 0)
            powerPage.flickable.contentY = Math.max(0, Math.min(position.y - 12,
                powerPage.flickable.contentHeight - powerPage.flickable.height))
            target.forceActiveFocus(Qt.TabFocusReason)
        })
        return true
    }

    property bool sortCoresByLoad: false
    readonly property bool hasGpuHardware: powerPage.controller.gpuAvailable && ((powerPage.controller.gpuDevices && powerPage.controller.gpuDevices.length > 0) || powerPage.controller.gpuBusyAvailable)
    readonly property bool hasBatteryHardware: powerPage.controller.batteryPresent

    readonly property PulseTheme theme: PulseTheme {
        id: theme
        bgColor: powerPage.Kirigami.Theme.backgroundColor
        disabledColor: powerPage.Kirigami.Theme.disabledTextColor
        negativeColor: powerPage.Kirigami.Theme.negativeTextColor
        neutralColor: powerPage.Kirigami.Theme.neutralTextColor
        positiveColor: powerPage.Kirigami.Theme.positiveTextColor
    }

    function orderedCoreItems() {
        var list = powerPage.controller.coreItems ? powerPage.controller.coreItems.slice() : []
        if (powerPage.sortCoresByLoad) {
            list.sort(function(a, b) {
                var diff = (Number(b.loadPercent) || 0) - (Number(a.loadPercent) || 0)
                return Math.abs(diff) > 0.1 ? diff : ((Number(a.coreId) || 0) - (Number(b.coreId) || 0))
            })
        }
        return list
    }

    function profileIcon(prof) {
        if (prof === "performance") return "speedometer"
        if (prof === "power-saver") return "battery-profile-powersave"
        return "battery-profile-performance"
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing
        // 1. Four Primary Power & Thermal Envelope Cards
        GridLayout {
            Layout.fillWidth: true
            columns: width < 680 ? 1 : 2
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            // Card 1: CPU Package & Power Limits
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: pkgCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: theme.cardRadius
                color: theme.surfaceColor
                border.width: 1
                border.color: theme.cardBorderColor

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    height: 2
                    radius: 1
                    color: theme.powerAccent
                    opacity: 0.7
                }

                ColumnLayout {
                    id: pkgCol
                    objectName: "section_cpu"
                    focus: true
                    Accessible.name: KI18n.i18n("cpu section")
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            radius: 6
                            color: theme.accentTint(theme.powerAccent, theme.isDark ? 0.18 : 0.12)
                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: 15
                                height: 15
                                source: "cpu"
                                color: theme.powerAccent
                            }
                        }
                        QQC2.Label {
                            text: KI18n.i18n("CPU package & power limits")
                            color: Kirigami.Theme.disabledTextColor
                            font.bold: true
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }

                    Kirigami.Heading {
                        level: 3
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: powerPage.controller.packagePowerAvailable
                              ? powerPage.controller.packageWatts.toFixed(1) + KI18n.i18n(" W · ")
                                + (powerPage.controller.packagePowerEstimated ? "Estimated" : "Measured")
                              : KI18n.i18n("CPU package power: Unavailable")
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        radius: 4
                        color: theme.trackColor
                        Rectangle {
                            visible: powerPage.controller.packagePowerAvailable
                            width: parent.width * Math.min(1.0, Math.max(0.0, powerPage.controller.packageWatts / Math.max(25.0, powerPage.controller.pl1LimitWatts > 0 ? powerPage.controller.pl1LimitWatts : 45.0)))
                            height: parent.height
                            radius: 4
                            color: theme.powerAccent
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: powerPage.controller.powerLimitsAvailable
                              ? KI18n.i18n("Power limits: PL1 ") + Math.round(powerPage.controller.pl1LimitWatts) + KI18n.i18n(" W · PL2 ")
                                + Math.round(powerPage.controller.pl2LimitWatts) + KI18n.i18n(" W")
                              : KI18n.i18n("Power limits: Unavailable")
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: powerPage.controller.deltaThrottleEvents > 0
                               ? Kirigami.Theme.negativeTextColor
                               : Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: powerPage.controller.throttleCountersAvailable
                              ? KI18n.i18n("Throttle events since boot: ") + powerPage.controller.totalThrottleEvents
                                + KI18n.i18n(" · New this interval: ") + powerPage.controller.deltaThrottleEvents
                              : KI18n.i18n("Throttle event counters: Unavailable")
                    }
                }
            }

            // Card 2: Graphics (GPU)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: gpuCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: theme.cardRadius
                color: theme.surfaceColor
                border.width: 1
                border.color: theme.cardBorderColor

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    height: 2
                    radius: 1
                    color: theme.gpuAccent
                    opacity: 0.7
                }

                ColumnLayout {
                    id: gpuCol
                    objectName: "section_gpu"
                    focus: true
                    Accessible.name: KI18n.i18n("gpu section")
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            radius: 6
                            color: theme.accentTint(theme.gpuAccent, theme.isDark ? 0.18 : 0.12)
                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: 15
                                height: 15
                                source: "video-display"
                                color: theme.gpuAccent
                            }
                        }
                        QQC2.Label {
                            text: KI18n.i18n("Graphics (GPU)")
                            color: Kirigami.Theme.disabledTextColor
                            font.bold: true
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        QQC2.Label {
                            visible: !powerPage.hasGpuHardware
                            text: KI18n.i18n("No GPU detected")
                            color: Kirigami.Theme.disabledTextColor
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }

                    Kirigami.Heading {
                        visible: powerPage.hasGpuHardware
                        level: 3
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: KI18n.i18n("GPU load: ") + (powerPage.controller.gpuBusyAvailable
                              ? Math.round(powerPage.controller.gpuBusyPercent) + "%" : KI18n.i18n("Unavailable"))
                              + KI18n.i18n(" · GPU temperature: ") + (powerPage.controller.gpuTempAvailable
                              ? powerPage.formatTemp(powerPage.controller.gpuTempCelsius) : KI18n.i18n("Unavailable"))
                    }

                    Rectangle {
                        visible: powerPage.hasGpuHardware
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        radius: 4
                        color: theme.trackColor
                        Rectangle {
                            visible: powerPage.controller.gpuBusyAvailable
                            width: parent.width * Math.min(1.0, Math.max(0.0, powerPage.controller.gpuBusyPercent / 100.0))
                            height: parent.height
                            radius: 4
                            color: theme.gpuAccent
                        }
                    }

                    QQC2.Label {
                        visible: powerPage.hasGpuHardware
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: KI18n.i18n("Driver: ") + (powerPage.controller.gpuAvailable && powerPage.controller.gpuDriverName
                              ? powerPage.controller.gpuDriverName : KI18n.i18n("Unavailable"))
                              + KI18n.i18n(" · Frequency: ") + (powerPage.controller.gpuFrequencyAvailable
                              ? powerPage.controller.gpuCurrentFreqMhz
                                + (powerPage.controller.gpuBoostFreqMhz > 0 ? " / " + powerPage.controller.gpuBoostFreqMhz : "") + KI18n.i18n(" MHz")
                              : KI18n.i18n("Unavailable"))
                    }

                    QQC2.Label {
                        visible: powerPage.hasGpuHardware
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: KI18n.i18n("VRAM: ") + (powerPage.controller.gpuVramAvailable
                              ? Math.round(powerPage.controller.gpuVramUsedMib) + " / " + Math.round(powerPage.controller.gpuVramTotalMib) + KI18n.i18n(" MiB")
                              : KI18n.i18n("Unavailable"))
                              + KI18n.i18n(" · GPU power: ") + (powerPage.controller.gpuPowerAvailable
                              ? powerPage.controller.gpuPowerWatts.toFixed(1) + KI18n.i18n(" W") : KI18n.i18n("Unavailable"))
                    }
                }
            }

            // Card 3: Battery & Power Source
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: batCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: theme.cardRadius
                color: theme.surfaceColor
                border.width: 1
                border.color: theme.cardBorderColor

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    height: 2
                    radius: 1
                    color: theme.psiAccent
                    opacity: 0.7
                }

                ColumnLayout {
                    id: batCol
                    objectName: "section_battery"
                    focus: true
                    Accessible.name: KI18n.i18n("battery section")
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            radius: 6
                            color: theme.accentTint(theme.psiAccent, theme.isDark ? 0.18 : 0.12)
                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: 15
                                height: 15
                                source: powerPage.hasBatteryHardware ? "battery-profile-performance" : "ac-adapter"
                                color: theme.psiAccent
                            }
                        }
                        QQC2.Label {
                            text: KI18n.i18n("Battery & power source")
                            color: Kirigami.Theme.disabledTextColor
                            font.bold: true
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        QQC2.Label {
                            visible: !powerPage.hasBatteryHardware
                            text: powerPage.controller.batteryStateAvailable
                                  ? (powerPage.controller.onAcPower ? KI18n.i18n("AC power · No battery") : KI18n.i18n("No battery"))
                                  : KI18n.i18n("No battery detected")
                            color: Kirigami.Theme.disabledTextColor
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }

                    Kirigami.Heading {
                        visible: powerPage.hasBatteryHardware
                        level: 3
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: KI18n.i18n("Power source: ") + (powerPage.controller.batteryStateAvailable
                              ? (powerPage.controller.onAcPower ? KI18n.i18n("AC power") : KI18n.i18n("Battery")) : KI18n.i18n("Unavailable"))
                    }

                    Rectangle {
                        visible: powerPage.hasBatteryHardware
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        radius: 4
                        color: theme.trackColor
                        Rectangle {
                            visible: powerPage.controller.batteryPresent && powerPage.controller.batteryLevelAvailable
                            width: parent.width * Math.min(1.0, Math.max(0.0, powerPage.controller.batteryPercent / 100.0))
                            height: parent.height
                            radius: 4
                            color: powerPage.controller.batteryPercent < 20 ? Kirigami.Theme.negativeTextColor : theme.psiAccent
                        }
                    }

                    QQC2.Label {
                        visible: powerPage.hasBatteryHardware
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: !powerPage.controller.batteryStateAvailable ? KI18n.i18n("Battery: Unavailable")
                              : (!powerPage.controller.batteryPresent ? KI18n.i18n("Battery: Not present · ") + powerPage.controller.batteryTimeRemainingText
                                 : KI18n.i18n("Battery: ") + (powerPage.controller.batteryLevelAvailable
                                   ? Math.round(powerPage.controller.batteryPercent) + "%" : KI18n.i18n("Level unavailable"))
                                   + " · " + powerPage.controller.batteryTimeRemainingText
                                   + (powerPage.controller.batteryPowerAvailable ? " · " + powerPage.controller.batteryWatts.toFixed(1) + KI18n.i18n(" W") : ""))
                    }

                    QQC2.Label {
                        visible: powerPage.hasBatteryHardware
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: powerPage.controller.batteryPresent
                              ? KI18n.i18n("Battery health: ") + (powerPage.controller.batteryHealthAvailable
                                ? Math.round(powerPage.controller.batteryHealthPercent) + "%" : KI18n.i18n("Unavailable"))
                              : KI18n.i18n("Battery health: Unavailable")
                    }
                }
            }

            // Card 4: Cooling & Device Thermal
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: coolCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: theme.cardRadius
                color: theme.surfaceColor
                border.width: 1
                border.color: theme.cardBorderColor

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    height: 2
                    radius: 1
                    color: theme.tempAccent
                    opacity: 0.7
                }

                ColumnLayout {
                    id: coolCol
                    objectName: "section_cooling"
                    focus: true
                    Accessible.name: KI18n.i18n("cooling section")
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            radius: 6
                            color: theme.accentTint(theme.tempAccent, theme.isDark ? 0.18 : 0.12)
                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: 15
                                height: 15
                                source: "temperature-normal"
                                color: theme.tempAccent
                            }
                        }
                        QQC2.Label {
                            text: KI18n.i18n("Cooling & device thermal")
                            color: Kirigami.Theme.disabledTextColor
                            font.bold: true
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }

                    Kirigami.Heading {
                        level: 3
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: KI18n.i18n("Fan speed: ") + (powerPage.controller.fanAvailable
                              ? powerPage.controller.primaryFanRpm + KI18n.i18n(" RPM") : KI18n.i18n("Unavailable"))
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        radius: 4
                        color: theme.trackColor
                        Rectangle {
                            visible: powerPage.controller.fanAvailable
                            width: parent.width * Math.min(1.0, Math.max(0.0, powerPage.controller.primaryFanRpm / 5500.0))
                            height: parent.height
                            radius: 4
                            color: theme.tempAccent
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: KI18n.i18n("NVMe temperature: ") + (powerPage.controller.nvmeTempAvailable
                              ? powerPage.formatTemp(powerPage.controller.nvmeTempCelsius) : KI18n.i18n("Unavailable"))
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: KI18n.i18n("Wireless temperature: ") + (powerPage.controller.wifiTempAvailable
                              ? powerPage.formatTemp(powerPage.controller.wifiTempCelsius) : KI18n.i18n("Unavailable"))
                    }
                }
            }
        }

        // 2. Segmented Power Profile Selector Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: profCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1
            border.color: theme.cardBorderColor

            ColumnLayout {
                id: profCol
                objectName: "section_profile"
                focus: true
                Accessible.name: KI18n.i18n("profile section")
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    Kirigami.Heading {
                        level: 3
                        text: KI18n.i18n("Power profile")
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: !powerPage.controller.powerProfileServiceAvailable
                              ? KI18n.i18n("Power profile service unavailable")
                              : KI18n.i18n("Active profile: ") + (powerPage.controller.activePowerProfile || KI18n.i18n("Unavailable"))
                        color: powerPage.controller.powerProfileServiceAvailable
                               ? Kirigami.Theme.highlightColor
                               : Kirigami.Theme.disabledTextColor
                        font.bold: true
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: powerPage.controller.availablePowerProfiles
                        delegate: PulseButton {
                            id: profBtn
                            required property string modelData
                            icon.name: powerPage.profileIcon(profBtn.modelData)
                            text: powerPage.controller.powerProfileChangePending ? "Applying…" : profBtn.modelData
                            checkable: true
                            checked: powerPage.controller.activePowerProfile === profBtn.modelData
                            enabled: powerPage.controller.powerProfileServiceAvailable
                                     && !powerPage.controller.powerProfileChangePending
                            Accessible.name: profBtn.modelData + (checked ? KI18n.i18n(", active profile") : KI18n.i18n(", change power profile"))
                            onClicked: powerPage.controller.setPowerProfile(profBtn.modelData)
                        }
                    }
                }
            }
        }

        // Multi-Device Selectors (Graphics & Batteries)
        GridLayout {
            Layout.fillWidth: true
            columns: width < 720 ? 1 : 2
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            // Graphics Devices Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: gpuDevCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: theme.cardRadius
                color: theme.surfaceColor
                border.width: 1
                border.color: theme.cardBorderColor

                ColumnLayout {
                    id: gpuDevCol
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Kirigami.Icon {
                            source: "video-display"
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                            color: theme.gpuAccent
                        }
                        Kirigami.Heading {
                            level: 3
                            text: KI18n.i18n("Graphics devices")
                            Layout.fillWidth: true
                        }
                    }

                    QQC2.Label {
                        visible: powerPage.controller.gpuDevices.length === 0
                        text: KI18n.i18n("No supported GPU telemetry interface found")
                        color: Kirigami.Theme.disabledTextColor
                    }

                    Repeater {
                        model: powerPage.controller.gpuDevices
                        delegate: QQC2.RadioButton {
                            id: gpuChoice
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.name + " · "
                                  + (modelData.busyAvailable ? Number(modelData.busyPercent).toFixed(0) + KI18n.i18n("% activity") : KI18n.i18n("Activity unavailable"))
                                  + " · " + (modelData.temperatureAvailable ? powerPage.formatTemp(modelData.tempCelsius) : KI18n.i18n("Temperature unavailable"))
                                  + (modelData.available ? "" : KI18n.i18n(" · Disconnected or unsupported"))
                            checked: powerPage.controller.selectedGpuId === modelData.id
                            onClicked: powerPage.controller.selectedGpuId = modelData.id
                            contentItem: QQC2.Label { text: gpuChoice.text; wrapMode: Text.WordWrap; leftPadding: gpuChoice.indicator.width + 8 }
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                        text: KI18n.i18n("Select a GPU for the live summary and recorded history. Availability depends on the driver.")
                    }
                }
            }

            // Individual Batteries Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: batDevCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: theme.cardRadius
                color: theme.surfaceColor
                border.width: 1
                border.color: theme.cardBorderColor

                ColumnLayout {
                    id: batDevCol
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Kirigami.Icon {
                            source: "battery-profile-performance"
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                            color: theme.batteryAccent
                        }
                        Kirigami.Heading {
                            level: 3
                            text: KI18n.i18n("Individual batteries")
                            Layout.fillWidth: true
                        }
                    }

                    QQC2.Label {
                        visible: powerPage.controller.batteries.length === 0
                        text: KI18n.i18n("No readable batteries present")
                        color: Kirigami.Theme.disabledTextColor
                    }

                    Repeater {
                        model: powerPage.controller.batteries
                        delegate: Rectangle {
                            id: batRow
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: bRowCol.implicitHeight + 10
                            radius: theme.innerRadius
                            color: theme.cardNestedBg

                            ColumnLayout {
                                id: bRowCol
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true
                                    QQC2.Label { text: batRow.modelData.name; font.bold: true; Layout.fillWidth: true }
                                    Rectangle {
                                        radius: theme.pillRadius
                                        color: theme.subtleSurfaceColor
                                        implicitWidth: bStatLbl.implicitWidth + 8
                                        implicitHeight: bStatLbl.implicitHeight + 4
                                        QQC2.Label { id: bStatLbl; anchors.centerIn: parent; text: batRow.modelData.status; font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2); font.bold: true }
                                    }
                                }

                                QQC2.Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    text: (batRow.modelData.available ? Number(batRow.modelData.percent).toFixed(0) + KI18n.i18n("% charge") : KI18n.i18n("Charge unavailable"))
                                          + " · " + (batRow.modelData.powerAvailable ? Number(batRow.modelData.watts).toFixed(1) + KI18n.i18n(" W") : KI18n.i18n("Power unavailable"))
                                }
                            }
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                        text: KI18n.i18n("Battery totals combine compatible energy readings only. Power-source status and battery charging are independent, including USB-C supplies.")
                    }
                }
            }
        }

        // 3. CPU Cores & Per-Core Heatmap Matrix
        RowLayout {
            Layout.fillWidth: true
            Kirigami.Heading {
                Layout.fillWidth: true
                level: 2
                id: coresHeading
                objectName: "section_cores"
                focus: true
                text: KI18n.i18n("CPU cores and sensors")
                wrapMode: Text.WordWrap
            }
            PulseButton {
                text: powerPage.sortCoresByLoad ? KI18n.i18n("Sorted by load") : KI18n.i18n("Sort by load")
                icon.name: "view-sort-descending"
                checkable: true
                checked: powerPage.sortCoresByLoad
                onClicked: powerPage.sortCoresByLoad = checked
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width < 560 ? 1 : (width < 900 ? 2 : 4)
            columnSpacing: Kirigami.Units.mediumSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            Repeater {
                model: powerPage.orderedCoreItems()
                delegate: Rectangle {
                    id: coreCard
                    required property var modelData
                    readonly property real loadRatio: coreCard.modelData.loadAvailable
                        ? Math.min(1.0, Math.max(0.0, Number(coreCard.modelData.loadPercent) / 100.0))
                        : 0.0
                    Layout.fillWidth: true
                    implicitHeight: coreInnerCol.implicitHeight + Kirigami.Units.mediumSpacing * 2
                    radius: theme.innerRadius
                    color: theme.surfaceColor
                    border.width: 1
                    border.color: theme.cardBorderColor

                    ColumnLayout {
                        id: coreInnerCol
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.mediumSpacing
                        spacing: 5

                        RowLayout {
                            Layout.fillWidth: true
                            QQC2.Label {
                                text: KI18n.i18n("CPU ") + coreCard.modelData.coreId
                                font.bold: true
                                color: Kirigami.Theme.textColor
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                visible: coreCard.modelData.tempCelsius > 0
                                radius: 4
                                color: theme.subtleSurfaceColor
                                implicitWidth: coreTempLbl.implicitWidth + 10
                                implicitHeight: coreTempLbl.implicitHeight + 4
                                QQC2.Label {
                                    id: coreTempLbl
                                    anchors.centerIn: parent
                                    text: powerPage.formatTemp(coreCard.modelData.tempCelsius)
                                    color: theme.tempAccent
                                    font.bold: true
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 7
                            radius: 3.5
                            color: theme.trackColor
                            Rectangle {
                                visible: coreCard.modelData.loadAvailable
                                width: parent.width * coreCard.loadRatio
                                height: parent.height
                                radius: 3.5
                                color: theme.cpuAccent
                            }
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.bold: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            text: KI18n.i18n("Load: ") + (coreCard.modelData.loadAvailable
                                  ? Math.round(coreCard.modelData.loadPercent) + "%" : KI18n.i18n("Unavailable"))
                                  + " · " + (coreCard.modelData.currentFreqMhz > 0
                                  ? coreCard.modelData.currentFreqMhz + KI18n.i18n(" MHz") : KI18n.i18n("Unavailable"))
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                            text: KI18n.i18n("Throttle count: ") + (coreCard.modelData.throttleCountersAvailable
                                  ? coreCard.modelData.throttleCount : KI18n.i18n("Unavailable"))
                        }
                    }
                }
            }
        }

        // 4. Detected Hardware Sensors Grid
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: sensorsCol.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: theme.cardRadius
            color: theme.surfaceColor
            border.width: 1
            border.color: theme.cardBorderColor

            ColumnLayout {
                id: sensorsCol
                objectName: "section_sensors"
                focus: true
                Accessible.name: KI18n.i18n("sensors section")
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                Kirigami.Heading {
                    level: 3
                    text: KI18n.i18n("Detected sensors")
                    Layout.fillWidth: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: powerPage.width < 680 ? 1 : 2
                    columnSpacing: Kirigami.Units.mediumSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: powerPage.controller.domainSensors
                        delegate: Rectangle {
                            id: sensorTile
                            required property var modelData
                            readonly property real tempRatio: sensorTile.modelData.available
                                ? Math.min(1.0, Math.max(0.0, (Number(sensorTile.modelData.currentCelsius) - 25.0) / Math.max(25.0, (Number(sensorTile.modelData.maxCelsius) || 100.0) - 25.0)))
                                : 0.0
                            Layout.fillWidth: true
                            implicitHeight: sensorTileCol.implicitHeight + 16
                            radius: theme.innerRadius
                            color: theme.subtleSurfaceColor
                            border.width: 1
                            border.color: theme.subtleBorderColor

                            ColumnLayout {
                                id: sensorTileCol
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    QQC2.Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        font.bold: true
                                        text: sensorTile.modelData.domainName + " · " + sensorTile.modelData.label + ": "
                                              + (sensorTile.modelData.available ? powerPage.formatTemp(sensorTile.modelData.currentCelsius) : KI18n.i18n("Unavailable"))
                                        Accessible.name: text
                                    }
                                    QQC2.Label {
                                        visible: sensorTile.modelData.limitsAvailable
                                        text: KI18n.i18n("Margin ") + MeasurementLabels.formatTemperatureDelta(Number(sensorTile.modelData.headroomCelsius), powerPage.controller.temperatureUnit)
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        color: Kirigami.Theme.disabledTextColor
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 5
                                    radius: 2.5
                                    color: theme.trackColor
                                    Rectangle {
                                        visible: sensorTile.modelData.available
                                        width: parent.width * sensorTile.tempRatio
                                        height: parent.height
                                        radius: 2.5
                                        color: Number(sensorTile.modelData.headroomCelsius) <= 5 && sensorTile.modelData.limitsAvailable
                                               ? Kirigami.Theme.neutralTextColor : theme.tempAccent
                                    }
                                }
                            }
                        }
                    }
                }

                QQC2.Label {
                    visible: powerPage.controller.domainSensors.length === 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.disabledTextColor
                    text: KI18n.i18n("No supported temperature sensors were found.")
                }
            }
        }
        MeasurementDetails {
            metadata: powerPage.controller.metricMetadata
        }

    }
}
