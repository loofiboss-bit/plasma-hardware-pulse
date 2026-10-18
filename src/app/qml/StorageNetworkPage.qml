pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    objectName: "storageNetworkPage"
    title: "Storage & Network"
    required property var controller
    function metric(value, suffix) { return value !== undefined && isFinite(Number(value)) ? Number(value).toFixed(2) + suffix : "Unavailable" }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing
        QQC2.Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: "Live device readings. Physical and virtual devices are listed individually; adding their readings can count the same traffic more than once. History retains the original device identity."
        }
        Kirigami.Heading { text: "Storage devices"; level: 2 }
        QQC2.RadioButton {
            text: "Use automatic physical device totals for storage history"
            checked: page.controller.selectedStorageId.length === 0
            onClicked: page.controller.selectedStorageId = ""
        }
        QQC2.Label { visible: page.controller.storageDevices.length === 0; text: "No readable storage devices" }
        Repeater {
            model: page.controller.storageDevices
            delegate: QQC2.GroupBox {
                id: disk
                required property var modelData
                title: modelData.name + (modelData.partition ? " · Partition" : modelData.physical ? " · Physical" : " · Virtual")
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    QQC2.Label {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: disk.modelData.available
                              ? "Read " + page.metric(disk.modelData.readMibPerSec, " MiB/s") + " · Write " + page.metric(disk.modelData.writeMibPerSec, " MiB/s")
                                + "\n" + page.metric(disk.modelData.iops, " IOPS") + " · Average request " + (disk.modelData.requestTimeAvailable ? page.metric(disk.modelData.requestMs, " ms") : "Unavailable")
                              : "Unavailable or disconnected — retained history remains available"
                    }
                    QQC2.RadioButton {
                        text: "Use this device for storage history"
                        checked: page.controller.selectedStorageId === disk.modelData.id
                        onClicked: page.controller.selectedStorageId = disk.modelData.id
                    }
                }
            }
        }
        Kirigami.Heading { text: "Network interfaces"; level: 2 }
        QQC2.Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: "Automatic totals use physical interfaces. Change the included interfaces in Settings. Error and drop counts are cumulative for the current interface identity."
        }
        Repeater {
            model: page.controller.networkInterfaces
            delegate: QQC2.GroupBox {
                id: net
                required property var modelData
                Layout.fillWidth: true
                title: modelData.name + (modelData.physical ? " · Physical" : " · Virtual") + (modelData.included ? " · Included" : "")
                QQC2.Label {
                    width: parent.width; wrapMode: Text.WordWrap
                    text: (net.modelData.available
                          ? "Receive " + page.metric(net.modelData.rxMibPerSec, " MiB/s") + " · Send " + page.metric(net.modelData.txMibPerSec, " MiB/s")
                          : "Unavailable or disconnected")
                          + (net.modelData.countersAvailable
                             ? "\nErrors: receive " + page.metric(net.modelData.rxErrors, "") + " · send " + page.metric(net.modelData.txErrors, "")
                               + "\nDropped packets: receive " + page.metric(net.modelData.rxDropped, "") + " · send " + page.metric(net.modelData.txDropped, "")
                             : "\nError and drop counters unavailable")
                }
            }
        }
        Kirigami.Heading { text: "Measurement capabilities"; level: 2 }
        Repeater {
            model: page.controller.capabilities
            delegate: QQC2.Label {
                required property var modelData
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: modelData.name + " · " + (modelData.available ? "Available" : "Unavailable") + "\n" + modelData.detail
            }
        }
    }
}
