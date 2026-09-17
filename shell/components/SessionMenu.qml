import Quickshell
import QtQml
import Quickshell.Wayland
import Quickshell.Io
import QtQuick
import "../services"

// Session actions overlay: lock / logout / reboot / poweroff.
// Commands come from Config.sessionCommands. Open via `quickshell ipc
// session toggle`, the "wolf" bar button, or wolfy.emit("session.open").
PanelWindow {
    id: root
    required property ShellScreen screen

    anchors { top: true; bottom: true; left: true; right: true }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: visible ? WlrKeyboardFocus.Exclusive
                                         : WlrKeyboardFocus.None
    color: "transparent"
    visible: false
    mask: Region { item: panel }

    IpcHandler {
        target: "session"
        function toggle() { root.visible = !root.visible; }
        function open()  { root.visible = true; }
        function close() { root.visible = false; }
    }

    Connections {
        target: ScriptHost
        function onSessionMenuRequested() { root.visible = true; }
    }

    Rectangle { // dim backdrop
        x: 0; y: 0; width: root.width; height: root.height
        color: Qt.rgba(0, 0, 0, 0.35)
        visible: root.visible
        MouseArea { anchors.fill: parent; onClicked: root.visible = false }
    }

    Row {
        id: panel
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        spacing: 16

        Repeater {
            model: [
                { "label": "Lock",     "key": "lock" },
                { "label": "Log out",  "key": "logout" },
                { "label": "Reboot",   "key": "reboot" },
                { "label": "Poweroff", "key": "poweroff" }
            ]
            delegate: Rectangle {
                required property var modelData
                width: 120; height: 80
                radius: Theme.radius
                color: btnMouse.containsMouse ? Theme.colors.overlay : Theme.colors.surface
                border.color: Theme.colors.border

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: Theme.colors.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize + 1
                }
                MouseArea {
                    id: btnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        const cmd = Config.sessionCommands[modelData.key];
                        if (cmd)
                            Quickshell.execDetached(["sh", "-c", cmd]);
                        root.visible = false;
                    }
                }
            }
        }
    }
}
