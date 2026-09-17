import Quickshell.Services.Pipewire
import QtQuick
import "../services"

// Loaded dynamically by VolumeControl; fails to load cleanly when
// QuickShell was built without the Pipewire service.
Item {
    id: root

    readonly property PwNode sink: Pipewire.defaultAudioSink
    readonly property int percent: sink && sink.audio
        ? Math.round(sink.audio.volume * 100) : -1
    readonly property bool muted: sink && sink.audio ? sink.audio.muted : false

    PwNodeLinkTracker { node: root.sink }

    function adjust(delta) {
        if (!root.sink || !root.sink.audio)
            return;
        const next = Math.min(1.5, Math.max(0, root.sink.audio.volume + delta / 100));
        root.sink.audio.muted = false;
        root.sink.audio.volume = next;
        ScriptHost.emitEvent("volume.changed", { "percent": Math.round(next * 100) });
    }

    function toggleMute() {
        if (root.sink && root.sink.audio) {
            root.sink.audio.muted = !root.sink.audio.muted;
            ScriptHost.emitEvent("volume.changed",
                { "percent": percent, "muted": root.sink.audio.muted });
        }
    }
}
