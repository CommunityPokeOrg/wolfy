import Quickshell.I3
import QtQuick
import "../services"

// sway/i3 workspace strip. Click switches; focused is highlighted,
// urgent flashes the error colour. Emits "workspace.changed" to scripts.
Row {
    id: root
    spacing: 4
    implicitWidth: childrenRect.width
    implicitHeight: childrenRect.height

    readonly property string focusedName: {
        for (const w of I3.workspaces)
            if (w.focused)
                return w.name;
        return "";
    }
    onFocusedNameChanged: ScriptHost.emitEvent(
        "workspace.changed", { "name": focusedName })

    Repeater {
        model: I3.workspaces

        Rectangle {
            required property var modelData
            width: Math.max(28, label.implicitWidth + 14)
            height: 24
            radius: Theme.radiusSmall
            color: modelData.focused ? Theme.colors.accent
                 : modelData.urgent ? Theme.colors.err
                 : wsMouse.containsMouse ? Theme.colors.overlay
                 : Theme.colors.surface

            Text {
                id: label
                anchors.centerIn: parent
                text: modelData.name
                color: modelData.focused ? Theme.colors.bg : Theme.colors.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }

            MouseArea {
                id: wsMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: I3.dispatch(`workspace ${modelData.name}`)
            }
        }
    }
}
