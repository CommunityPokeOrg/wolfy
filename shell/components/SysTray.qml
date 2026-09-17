import Quickshell
import QtQml
import Quickshell.Services.SystemTray
import Quickshell.Widgets
import QtQuick
import "../services"

Row {
    spacing: 6
    implicitWidth: childrenRect.width
    implicitHeight: childrenRect.height

    Repeater {
        model: SystemTray.items

        Item {
            required property var modelData
            width: 20
            height: 20

            IconImage {
                anchors.fill: parent
                source: modelData.icon
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.LeftButton)
                        modelData.activate();
                    else if (modelData.hasMenu)
                        modelData.display(null, 0, 0);
                }
            }
        }
    }
}
