import Quickshell.Hyprland
import QtQuick
import "../services"

// Hyprland workspace strip, selected when Config.compositor == "hyprland".
Row {
    id: root
    spacing: 4
    implicitWidth: childrenRect.width
    implicitHeight: childrenRect.height

    readonly property string focusedName: Hyprland.focusedWorkspace
        ? Hyprland.focusedWorkspace.name : ""
    onFocusedNameChanged: ScriptHost.emitEvent(
        "workspace.changed", { "name": focusedName })

    Repeater {
        model: Hyprland.workspaces

        Rectangle {
            required property var modelData
            width: Math.max(28, label.implicitWidth + 14)
            height: 24
            radius: Theme.radiusSmall
            color: modelData.active ? Theme.colors.accent
                 : wsMouse.containsMouse ? Theme.colors.overlay
                 : Theme.colors.surface

            Text {
                id: label
                anchors.centerIn: parent
                text: modelData.name
                color: modelData.active ? Theme.colors.bg : Theme.colors.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }

            MouseArea {
                id: wsMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: modelData.activate()
            }
        }
    }
}
