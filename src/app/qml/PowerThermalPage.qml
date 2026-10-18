pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: powerPage
    objectName: "powerPage"
    title: "Power & Cooling"
    required property var controller

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
                activeFocusOnTab: true
                Accessible.name: "cpu section"
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
                            text: "CPU package & power limits"
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
                              ? powerPage.controller.packageWatts.toFixed(1) + " W · "
                                + (powerPage.controller.packagePowerEstimated ? "Estimated" : "Measured")
                              : "CPU package power: Unavailable"
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
                              ? "Power limits: PL1 " + Math.round(powerPage.controller.pl1LimitWatts) + " W · PL2 "
                                + Math.round(powerPage.controller.pl2LimitWatts) + " W"
                              : "Power limits: Unavailable"
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: powerPage.controller.deltaThrottleEvents > 0
                               ? Kirigami.Theme.negativeTextColor
                               : Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: powerPage.controller.throttleCountersAvailable
                              ? "Throttle events since boot: " + powerPage.controller.totalThrottleEvents
                                + " · New this interval: " + powerPage.controller.deltaThrottleEvents
                              : "Throttle event counters: Unavailable"
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
                activeFocusOnTab: true
                Accessible.name: "gpu section"
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
                            text: "Graphics (GPU)"
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
                        text: "GPU load: " + (powerPage.controller.gpuBusyAvailable
                              ? Math.round(powerPage.controller.gpuBusyPercent) + "%" : "Unavailable")
                              + " · GPU temperature: " + (powerPage.controller.gpuTempAvailable
                              ? Math.round(powerPage.controller.gpuTempCelsius) + " °C" : "Unavailable")
                    }

                    Rectangle {
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
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: "Driver: " + (powerPage.controller.gpuAvailable && powerPage.controller.gpuDriverName
                              ? powerPage.controller.gpuDriverName : "Unavailable")
                              + " · Frequency: " + (powerPage.controller.gpuFrequencyAvailable
                              ? powerPage.controller.gpuCurrentFreqMhz
                                + (powerPage.controller.gpuBoostFreqMhz > 0 ? " / " + powerPage.controller.gpuBoostFreqMhz : "") + " MHz"
                              : "Unavailable")
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: "VRAM: " + (powerPage.controller.gpuVramAvailable
                              ? Math.round(powerPage.controller.gpuVramUsedMib) + " / " + Math.round(powerPage.controller.gpuVramTotalMib) + " MiB"
                              : "Unavailable")
                              + " · GPU power: " + (powerPage.controller.gpuPowerAvailable
                              ? powerPage.controller.gpuPowerWatts.toFixed(1) + " W" : "Unavailable")
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
                activeFocusOnTab: true
                Accessible.name: "battery section"
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
                                source: "battery-profile-performance"
                                color: theme.psiAccent
                            }
                        }
                        QQC2.Label {
                            text: "Battery & power source"
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
                        text: "Power source: " + (powerPage.controller.batteryStateAvailable
                              ? (powerPage.controller.onAcPower ? "AC power" : "Battery") : "Unavailable")
                    }

                    Rectangle {
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
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: !powerPage.controller.batteryStateAvailable ? "Battery: Unavailable"
                              : (!powerPage.controller.batteryPresent ? "Battery: Not present · " + powerPage.controller.batteryTimeRemainingText
                                 : "Battery: " + (powerPage.controller.batteryLevelAvailable
                                   ? Math.round(powerPage.controller.batteryPercent) + "%" : "Level unavailable")
                                   + " · " + powerPage.controller.batteryTimeRemainingText
                                   + (powerPage.controller.batteryPowerAvailable ? " · " + powerPage.controller.batteryWatts.toFixed(1) + " W" : ""))
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: powerPage.controller.batteryPresent
                              ? "Battery health: " + (powerPage.controller.batteryHealthAvailable
                                ? Math.round(powerPage.controller.batteryHealthPercent) + "%" : "Unavailable")
                              : "Battery health: Unavailable"
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
                activeFocusOnTab: true
                Accessible.name: "cooling section"
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
                            text: "Cooling & device thermal"
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
                        text: "Fan speed: " + (powerPage.controller.fanAvailable
                              ? powerPage.controller.primaryFanRpm + " RPM" : "Unavailable")
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
                        text: "NVMe temperature: " + (powerPage.controller.nvmeTempAvailable
                              ? Math.round(powerPage.controller.nvmeTempCelsius) + " °C" : "Unavailable")
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: "Wireless temperature: " + (powerPage.controller.wifiTempAvailable
                              ? Math.round(powerPage.controller.wifiTempCelsius) + " °C" : "Unavailable")
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
                activeFocusOnTab: true
                Accessible.name: "profile section"
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    Kirigami.Heading {
                        level: 3
                        text: "Power profile"
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: !powerPage.controller.powerProfileServiceAvailable
                              ? "Power profile service unavailable"
                              : "Active profile: " + (powerPage.controller.activePowerProfile || "Unavailable")
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
                        delegate: QQC2.Button {
                            id: profBtn
                            required property string modelData
                            icon.name: powerPage.profileIcon(profBtn.modelData)
                            text: powerPage.controller.powerProfileChangePending ? "Applying…" : profBtn.modelData
                            checkable: true
                            checked: powerPage.controller.activePowerProfile === profBtn.modelData
                            enabled: powerPage.controller.powerProfileServiceAvailable
                                     && !powerPage.controller.powerProfileChangePending
                            Accessible.name: profBtn.modelData + (checked ? ", active profile" : ", change power profile")
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
                            text: "Graphics devices"
                            Layout.fillWidth: true
                        }
                    }

                    QQC2.Label {
                        visible: powerPage.controller.gpuDevices.length === 0
                        text: "No supported GPU telemetry interface found"
                        color: Kirigami.Theme.disabledTextColor
                    }

                    Repeater {
                        model: powerPage.controller.gpuDevices
                        delegate: QQC2.RadioButton {
                            id: gpuChoice
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.name + " · "
                                  + (modelData.busyAvailable ? Number(modelData.busyPercent).toFixed(0) + "% activity" : "Activity unavailable")
                                  + " · " + (modelData.temperatureAvailable ? Number(modelData.tempCelsius).toFixed(1) + " °C" : "Temperature unavailable")
                                  + (modelData.available ? "" : " · Disconnected or unsupported")
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
                        text: "Select a GPU for the live summary and recorded history. Availability depends on the driver."
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
                            text: "Individual batteries"
                            Layout.fillWidth: true
                        }
                    }

                    QQC2.Label {
                        visible: powerPage.controller.batteries.length === 0
                        text: "No readable batteries present"
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
                                    text: (batRow.modelData.available ? Number(batRow.modelData.percent).toFixed(0) + "% charge" : "Charge unavailable")
                                          + " · " + (batRow.modelData.powerAvailable ? Number(batRow.modelData.watts).toFixed(1) + " W" : "Power unavailable")
                                }
                            }
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                        text: "Battery totals combine compatible energy readings only. Power-source status and battery charging are independent, including USB-C supplies."
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
                activeFocusOnTab: true
                text: "CPU cores and sensors"
                wrapMode: Text.WordWrap
            }
            QQC2.Button {
                text: powerPage.sortCoresByLoad ? "Sorted by load" : "Sort by load"
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
                                text: "CPU " + coreCard.modelData.coreId
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
                                    text: Math.round(coreCard.modelData.tempCelsius) + " °C"
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
                            text: "Load: " + (coreCard.modelData.loadAvailable
                                  ? Math.round(coreCard.modelData.loadPercent) + "%" : "Unavailable")
                                  + " · " + (coreCard.modelData.currentFreqMhz > 0
                                  ? coreCard.modelData.currentFreqMhz + " MHz" : "Unavailable")
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                            text: "Throttle count: " + (coreCard.modelData.throttleCountersAvailable
                                  ? coreCard.modelData.throttleCount : "Unavailable")
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
                activeFocusOnTab: true
                Accessible.name: "sensors section"
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                Kirigami.Heading {
                    level: 3
                    text: "Detected sensors"
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
                                              + (sensorTile.modelData.available ? Number(sensorTile.modelData.currentCelsius).toFixed(1) + " °C" : "Unavailable")
                                        Accessible.name: text
                                    }
                                    QQC2.Label {
                                        visible: sensorTile.modelData.limitsAvailable
                                        text: "Margin " + Math.round(Number(sensorTile.modelData.headroomCelsius)) + "°C"
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
                    text: "No supported temperature sensors were found."
                }
            }
        }
        MeasurementDetails {
            metadata: powerPage.controller.metricMetadata
        }

    }
}
