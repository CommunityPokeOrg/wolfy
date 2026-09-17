import QtQuick
import QtQml
import "../services"

// Notification indicator + DND toggle. Click = clear all,
// right-click = toggle do-not-disturb.
Item {
    implicitWidth: label.implicitWidth + 10
    implicitHeight: label.implicitHeight

    Text {
        id: label
        anchors.centerIn: parent
        text: Notif.doNotDisturb ? "DND"
            : (Notif.model.count > 0 ? "bell " + Notif.model.count : "bell")
        color: Notif.doNotDisturb ? Theme.colors.subtext : Theme.colors.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton)
                Notif.doNotDisturb = !Notif.doNotDisturb;
            else
                Notif.dismissAll();
        }
    }
}
