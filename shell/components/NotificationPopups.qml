import Quickshell
import Quickshell.Wayland
import QtQml
import QtQuick
import "../services"

// Transient popups, top-right, non-exclusive. Renders Notif.model;
// each row dismisses itself after Config.popupSeconds.
PanelWindow {
    id: root
    required property ShellScreen screen

    anchors { top: true; right: true }
    implicitWidth: 360
    implicitHeight: column.implicitHeight + 16
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    color: "transparent"
    mask: Region { item: column }

    Column {
        id: column
        x: root.width - width - 8
        y: 44
        spacing: 8

        Repeater {
            model: Notif.model

            Rectangle {
                required property int index
                required property var model
                width: 344
                height: bodyText.implicitHeight + summaryText.implicitHeight + 28
                radius: Theme.radius
                color: Theme.colors.surface
                border.color: Theme.colors.border
                border.width: 1

                Timer {
                    interval: Config.popupSeconds * 1000
                    running: true
                    onTriggered: Notif.dismiss(index)
                }

                Text {
                    id: summaryText
                    anchors { top: parent.top; left: parent.left; right: parent.right; margins: 10 }
                    text: (model.appName ? model.appName + "  —  " : "") + model.summary
                    color: Theme.colors.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    id: bodyText
                    anchors { top: summaryText.bottom; left: parent.left; right: parent.right;
                              margins: 10; topMargin: 4 }
                    text: model.body
                    color: Theme.colors.subtext
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 1
                    wrapMode: Text.Wrap
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: Notif.dismiss(index)
                }
            }
        }
    }
}
