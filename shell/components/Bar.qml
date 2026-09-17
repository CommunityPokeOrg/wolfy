import Quickshell
import QtQuick
import QtQuick.Layouts
import "../services"

// Top (or bottom) panel. One per screen via Variants in shell.qml.
PanelWindow {
    id: bar
    required property ShellScreen screen

    anchors {
        top: Config.barPosition === "top"
        bottom: Config.barPosition === "bottom"
        left: true
        right: true
    }
    implicitHeight: Config.barHeight
    exclusiveZone: implicitHeight
    color: "transparent"

    Rectangle {
        anchors.fill: parent
        anchors.margins: 4
        radius: Theme.radius
        color: Theme.colors.bg
        opacity: Config.barOpacity
        border.color: Theme.colors.border
        border.width: 1
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: Theme.spacing

        // left: workspaces
        Workspaces { Layout.fillHeight: true }

        Item { Layout.fillWidth: true }

        // center: clock
        Clock { Layout.fillHeight: true }

        Item { Layout.fillWidth: true }

        // right: tray + status + session
        RowLayout {
            Layout.fillHeight: true
            spacing: Theme.spacing
            SysTray { Layout.fillHeight: true }
            Battery { Layout.fillHeight: true }
            VolumeControl { Layout.fillHeight: true }
            NotifBell { Layout.fillHeight: true }
            SessionButton { Layout.fillHeight: true }
        }
    }
}
