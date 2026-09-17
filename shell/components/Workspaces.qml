import QtQuick
import "../services"

// Compositor workspaces. Selects the backend from Config.compositor:
// "sway"/"i3" -> WorkspacesI3.qml, "hyprland" -> WorkspacesHyprland.qml.
Item {
    id: root
    implicitWidth: loader.implicitWidth
    implicitHeight: loader.implicitHeight

    Loader {
        id: loader
        source: Config.compositor === "hyprland"
            ? "WorkspacesHyprland.qml" : "WorkspacesI3.qml"
    }
}
