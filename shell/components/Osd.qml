import Quickshell
import QtQml
import Quickshell.Wayland
import QtQuick
import "../services"

// On-screen display. Shown briefly when scripts emit "shell.osd"
// ({icon, label, value 0..100}) or volume/brightness changes arrive.
PanelWindow {
    id: root
    required property ShellScreen screen

    anchors { bottom: true }
    margins { bottom: 80 }
    implicitWidth: 240
    implicitHeight: 64
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    color: "transparent"
    visible: false
    mask: Region {}   // never takes input

    property string icon: ""
    property string label: ""
    property int value: -1

    Connections {
        target: ScriptHost
        function onOsdRequested(p) {
            root.icon = p.icon || "";
            root.label = p.label || "";
            root.value = p.value === undefined ? -1 : p.value;
            root.visible = true;
            hideTimer.restart();
        }
    }

    Timer {
        id: hideTimer
        interval: 1500
        onTriggered: root.visible = false
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: Theme.colors.surface
        border.color: Theme.colors.border
        opacity: 0.95

        Text {
            anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 8 }
            text: (root.icon ? root.icon + "  " : "") + root.label
            color: Theme.colors.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
        }

        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: 14 }
            height: 6
            radius: 3
            color: Theme.colors.overlay
            visible: root.value >= 0

            Rectangle {
                width: parent.width * Math.min(100, root.value) / 100
                height: parent.height
                radius: 3
                color: Theme.colors.accent
            }
        }
    }
}
