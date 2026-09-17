import Quickshell
import QtQml
import Quickshell.Services.SystemTray
import Quickshell.Widgets
import QtQuick
import "../services"

Item {
    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    Row {
        id: row
        spacing: 6

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
}
