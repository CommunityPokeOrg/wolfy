import QtQuick
import QtQml
import "../services"

// Volume indicator. Uses Pipewire when available; otherwise shows "--"
// and still reacts to script-driven events. Scroll adjusts volume,
// click toggles mute.
Item {
    id: root
    implicitWidth: label.implicitWidth + 14
    implicitHeight: label.implicitHeight

    // Set by VolumePipewire.qml when the Pipewire service is available.
    property int volume: -1        // 0..100, -1 = unavailable
    property bool muted: false
    // var on purpose: the loaded item is an interface (percent/muted/
    // adjust/toggleMute) supplied by whichever implementation loads.
    readonly property var pipewire: pw.item

    Loader {
        id: pw
        source: "VolumePipewire.qml"
        onStatusChanged: {
            if (pw.status === Loader.Error) {
                console.warn("[wolfy] Pipewire service unavailable; volume read-only");
            } else if (pw.status === Loader.Ready) {
                root.volume = Qt.binding(() => root.pipewire.percent);
                root.muted = Qt.binding(() => root.pipewire.muted);
            }
        }
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.volume < 0 ? "vol --" : (root.muted ? "muted" : "vol " + root.volume + "%")
        color: root.muted ? Theme.colors.subtext : Theme.colors.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: if (root.pipewire) root.pipewire.toggleMute()
        onWheel: (wheel) => {
            if (!root.pipewire)
                return;
            root.pipewire.adjust(wheel.angleDelta.y > 0 ? 5 : -5);
        }
    }
}
