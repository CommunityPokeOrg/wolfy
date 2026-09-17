import Quickshell.Services.UPower
import QtQuick
import "../services"

// Battery pill. Hidden automatically on systems with no battery.
Item {
    id: root
    visible: UPower.displayDevice.isLaptopBattery
    implicitWidth: visible ? label.implicitWidth + 14 : 0
    implicitHeight: label.implicitHeight

    readonly property int percent: Math.round(UPower.displayDevice.percentage * 100)
    readonly property bool charging: UPower.displayDevice.state === UPowerDeviceState.Charging

    onPercentChanged: ScriptHost.emitEvent(
        "battery.level", { "percent": percent, "charging": charging })

    Text {
        id: label
        anchors.centerIn: parent
        text: root.percent + "%" + (root.charging ? " +" : "")
        color: root.percent <= 20 && !root.charging ? Theme.colors.err
             : Theme.colors.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize
    }
}
