import Quickshell
import Quickshell.Wayland
import QtQuick
import "../services"

// Background layer. Uses Config.wallpaper (image path) if set,
// otherwise a theme-coloured vertical gradient.
PanelWindow {
    id: root
    required property ShellScreen screen

    anchors { top: true; bottom: true; left: true; right: true }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Background
    color: Theme.colors.bg

    Image {
        anchors.fill: parent
        source: Config.wallpaper !== "" ? "file://" + Config.wallpaper : ""
        fillMode: Image.PreserveAspectCrop
        visible: Config.wallpaper !== ""
    }

    Rectangle {
        anchors.fill: parent
        visible: Config.wallpaper === ""
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.colors.bg }
            GradientStop { position: 1.0; color: Theme.colors.surface }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: Config.wallpaper === ""
        text: "W O L F Y"
        color: Qt.rgba(Theme.colors.text.r, Theme.colors.text.g,
                       Theme.colors.text.b, 0.06)
        font.family: Theme.fontFamily
        font.pixelSize: 96
        font.letterSpacing: 24
    }
}
