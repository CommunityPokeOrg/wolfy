// window-sync.js — demonstration of the Wolfy window-sync API.
//
// - Logs every window.opened / window.adopted / window.closed event.
// - Posts a notification when a window is adopted (state came from
//   another source/tombstone rather than first-open setup).
// - Optionally ensures the KWin bridge stays loaded when
//   config.windowSync.kwinBridge is enabled.

wolfy.on("shell.start", function () {
    wolfy.log("window-sync: registry has", wolfy.sync.windows().length, "entries");
    if (wolfy.config && wolfy.config.windowSync && wolfy.config.windowSync.kwinBridge) {
        wolfy.exec("tools/kwin-bridge.sh ensure", function (out, code, err) {
            if (code !== 0) wolfy.warn("window-sync: kwin bridge ensure failed:", err || out);
            else wolfy.log("window-sync: kwin bridge ensured");
        });
    }
});

wolfy.on("window.opened", function (e) {
    wolfy.log("window opened:", e.appId || "?", "-", e.title || "");
});

wolfy.on("window.adopted", function (e) {
    var prev = e.previous || {};
    wolfy.log("window adopted:", e.window.appId,
              "from", prev.source, "workspace", prev.workspace || "?");
    wolfy.emit("shell.notify", {
        summary: "Window adopted",
        body: (e.window.appId || "?") + " restored from " + (prev.source || "history"),
        appName: "wolfy"
    });
});

wolfy.on("window.closed", function (e) {
    wolfy.log("window closed:", e.appId || "?", e.key || "");
});

wolfy.on("sync.connected", function (e) {
    wolfy.log("window-sync bridge", e.connected ? "connected" : "disconnected");
});
