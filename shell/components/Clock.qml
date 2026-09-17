import Quickshell
import QtQml
import QtQuick
import "../services"

Item {
    implicitWidth: label.implicitWidth + 16
    implicitHeight: label.implicitHeight

    SystemClock {
        id: clock
        precision: SystemClock.Seconds
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: Qt.formatDateTime(clock.date, "ddd dd MMM  HH:mm:ss")
        color: Theme.colors.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize
    }
}
