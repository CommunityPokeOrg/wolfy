import QtQuick
import "../services"

// Cross-source taskbar: one button per live registry window, fed by
// WinSync — local toplevels (source "local") plus anything pushed over
// D-Bus (source "kwin", other views). Local windows activate on click;
// remote entries emit "window.activate" so a script can route the
// request to the owning compositor (the registry can mirror state but
// cannot move a window's surface between compositors).
Item {
    id: root
    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    visible: Config.windowSync.enabled

    Row {
        id: row
        spacing: 4

        Repeater {
            model: {
                const out = [];
                if (WinSync.sync.enabled) {
                    for (const e of WinSync.sync.windows)
                        if (e && e.gone !== true)
                            out.push(e);
                }
                return out;
            }

            Rectangle {
                required property var modelData
                width: Math.min(200, Math.max(40, label.implicitWidth + 30))
                height: 24
                radius: Theme.radiusSmall
                color: modelData.focused ? Theme.colors.accent
                     : taskMouse.containsMouse ? Theme.colors.overlay
                     : Theme.colors.surface
                opacity: modelData.minimized ? 0.55 : 1.0

                Text {
                    id: label
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 7
                    width: parent.width - 14
                    elide: Text.ElideRight
                    text: (modelData.source && modelData.source !== "local"
                           ? "[" + modelData.source + "] " : "")
                          + (modelData.title || modelData.appId || modelData.id || "?")
                    color: modelData.focused ? Theme.colors.bg : Theme.colors.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 2
                }

                MouseArea {
                    id: taskMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: WinSync.activate(modelData.key, modelData)
                }
            }
        }
    }
}
