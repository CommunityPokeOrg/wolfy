// Show the OSD whenever the volume changes (Pipewire backend emits
// "volume.changed"; compositor keybinds can emit it too via IPC).

wolfy.on("volume.changed", function (data) {
    wolfy.emit("shell.osd", {
        icon: data.muted ? "muted" : "vol",
        label: data.muted ? "Volume muted" : "Volume " + data.percent + "%",
        value: data.muted ? 0 : data.percent
    });
});
