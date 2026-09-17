import Quickshell
import Quickshell.Wayland
import Quickshell.Widgets
import QtQuick
import "../services"

// App launcher overlay. Open via `quickshell ipc launcher toggle`,
// the bar, or wolfy.emit("launcher.open") from a script.
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
    // Input only where the panel box is; clicks outside close it.
    mask: Region { item: panel }

    IpcHandler {
        target: "launcher"
        function toggle() { root.visible = !root.visible; if (root.visible) input.forceActiveFocus(); }
        function open()  { root.visible = true;  input.forceActiveFocus(); }
        function close() { root.visible = false; }
    }

    Connections {
        target: ScriptHost
        function onLauncherRequested() { root.visible = true; input.forceActiveFocus(); }
    }

    // Filtered application model.
    readonly property var apps: DesktopEntries.applications.values
    function matches(entry, query) {
        const q = query.toLowerCase();
        return entry.name.toLowerCase().includes(q)
            || (entry.genericName || "").toLowerCase().includes(q);
    }
    readonly property var filtered: {
        const q = input.text;
        const all = root.apps;
        const out = [];
        for (const e of all) {
            if (q === "" || matches(e, q)) {
                out.push(e);
                if (out.length >= Config.launcherMaxResults)
                    break;
            }
        }
        return out;
    }

    Rectangle {
        id: panel
        x: Math.round((root.width - width) / 2)
        y: Math.max(80, Math.round(root.height * 0.2))
        width: 480
        height: inputBox.height + list.height + 24
        radius: Theme.radius
        color: Theme.colors.surface
        border.color: Theme.colors.border
        border.width: 1

        Rectangle {
            id: inputBox
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 12 }
            height: input.implicitHeight + 18
            radius: Theme.radiusSmall
            color: Theme.colors.bg
            border.color: input.activeFocus ? Theme.colors.accent : Theme.colors.border

            TextInput {
                id: input
                anchors { left: parent.left; right: parent.right;
                          verticalCenter: parent.verticalCenter; margins: 9 }
                color: Theme.colors.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 2
                clip: true
                Keys.onEscapePressed: root.visible = false
                Keys.onReturnPressed: {
                    if (root.filtered.length > 0) {
                        root.filtered[0].execute();
                        root.visible = false;
                    }
                }
                Keys.onDownPressed: list.incrementCurrentIndex()
                Keys.onUpPressed: list.decrementCurrentIndex()
            }
            Text {
                anchors { left: parent.left; leftMargin: 9; verticalCenter: parent.verticalCenter }
                visible: input.text === ""
                text: "launch..."
                color: Theme.colors.subtext
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 2
            }
        }

        ListView {
            id: list
            anchors { top: inputBox.bottom; left: parent.left; right: parent.right;
                      margins: 12; topMargin: 6 }
            height: Math.min(contentHeight, 360)
            clip: true
            model: root.filtered
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: list.width
                height: 40
                radius: Theme.radiusSmall
                color: index === list.currentIndex || itemMouse.containsMouse
                    ? Theme.colors.overlay : "transparent"

                IconImage {
                    id: icon
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 24; height: 24
                    source: Quickshell.iconPath(modelData.icon || "", true)
                }
                Text {
                    anchors { left: icon.right; leftMargin: 10; verticalCenter: parent.verticalCenter }
                    text: modelData.name
                    color: Theme.colors.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                }
                MouseArea {
                    id: itemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: { modelData.execute(); root.visible = false; }
                }
            }
        }
    }
}
