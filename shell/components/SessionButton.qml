import QtQuick
import "../services"

Item {
    implicitWidth: label.implicitWidth + 10
    implicitHeight: label.implicitHeight

    Text {
        id: label
        anchors.centerIn: parent
        text: "wolf"
        color: Theme.colors.accent
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize
        font.bold: true
    }

    MouseArea {
        anchors.fill: parent
        onClicked: ScriptHost.sessionMenuRequested()
    }
}
