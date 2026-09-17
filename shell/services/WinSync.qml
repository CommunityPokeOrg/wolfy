pragma Singleton
import Quickshell
import Quickshell.Wayland
import Quickshell.I3
import Wolfy.Core 1.0
import QtQml
import QtQuick

// WinSync owns the window registry and feeds the local compositor's
// toplevels into it. A window that appears gets its state *adopted*
// from the registry (KWin bridge, another view, or persisted tombstone)
// instead of being treated as brand new — so view/display switches
// don't re-run per-window setup.
Singleton {
    id: root

    readonly property alias sync: sync
    readonly property bool bridgeConnected: sync.bridgeConnected

    // toplevel object -> registry key ("local|<n>")
    property var liveKeys: new Map()
    property int nextLocalId: 1

    WindowSync {
        id: sync
        enabled: Config.windowSync.enabled
        persistPath: Config.windowSync.persist
            ? Config.dir + "/window-sync.json" : ""
        adoptTtlMs: Config.windowSync.adoptTtlMs
        suppressMs: Config.windowSync.suppressMs
        staleMs: Config.windowSync.staleMs

        onWindowAdded: function(e)   { root.forward("window.added", e) }
        onWindowUpdated: function(e) { root.forward("window.updated", e) }
        onWindowRemoved: function(e) { root.forward("window.closed", e) }
        onWindowAdopted: function(e) { /* handled in adoptWindow() */ }
        onBridgeConnectedChanged:
            ScriptHost.emitEvent("sync.connected", { connected: sync.bridgeConnected })
        onLogMessage: function(level, msg) { console.log(`[wolfy:sync] ${msg}`) }
    }

    function forward(name, entry) {
        // Only local events get the full window object back; remote
        // sources emit the raw registry entry.
        ScriptHost.emitEvent(name, entry);
    }

    // Local toplevel tracking -------------------------------------------
    // The wlr foreign-toplevel model has no per-row signals we can wire
    // generically, so reconcile() runs on a short timer plus on model
    // changes; each pass diffs live ids against liveKeys.
    Timer {
        interval: 2000
        running: sync.enabled
        repeat: true
        onTriggered: root.reconcile()
    }

    Connections {
        target: ToplevelManager.toplevels
        function onValuesChanged() { root.reconcile() }
    }

    Component.onCompleted: {
        ScriptHost.engine.setWindowSync(sync);
        reconcile();
    }

    function entryFor(t) {
        const key = liveKeys.get(t);
        return {
            source: "local",
            id: String(key).split("|")[1],
            appId: t.appId || "",
            title: t.title || "",
            workspace: root.currentWorkspace(),
            output: (t.screens && t.screens.length > 0) ? String(t.screens[0].name) : "",
            minimized: !!t.minimized,
            maximized: !!t.maximized,
            fullscreen: !!t.fullscreen,
            focused: !!t.activated,
            rect: {}
        };
    }

    function currentWorkspace() {
        if (Config.compositor !== "sway")
            return "";
        for (const w of I3.workspaces.values)
            if (w.focused)
                return w.name;
        return "";
    }

    // Taskbar click: activate the toplevel behind a local registry key.
    // Remote (kwin|…) keys can't be activated here — the window lives on
    // the other compositor — so the click is forwarded as an event for a
    // script to route (e.g. wmctrl on the host session).
    function activate(key, entry) {
        for (const [t, k] of liveKeys) {
            if (k === key) {
                t.activate();
                return true;
            }
        }
        ScriptHost.emitEvent("window.activate",
                             entry !== undefined ? entry : { key: key });
        return false;
    }

    function reconcile() {
        if (!sync.enabled)
            return;
        const list = ToplevelManager.toplevels.values;
        const seen = new Set();
        for (const t of list) {
            if (!liveKeys.has(t))
                liveKeys.set(t, "local|" + (nextLocalId++));
            const key = liveKeys.get(t);
            seen.add(key);
            const wasKnown = !!sync.find("local|" + key.split("|")[1]);
            const entry = sync.upsert(entryFor(t));
            if (!wasKnown)
                adoptWindow(t, entry);
        }
        for (const [t, key] of liveKeys) {
            if (!seen.has(key)) {
                liveKeys.delete(t);
                sync.remove(key);
            }
        }
    }

    // Adoption: claim the freshest known state for this app that is NOT
    // the local entry itself. A hit means this window existed elsewhere
    // (or before a restart) — emit window.adopted and restore, instead of
    // window.opened which would re-run first-open setup.
    function adoptWindow(t, localEntry) {
        const identity = localEntry.identity || "";
        if (identity === "") {
            ScriptHost.emitEvent("window.opened", localEntry);
            return;
        }
        const prev = sync.adopt(identity, localEntry.key);
        if (Object.keys(prev).length === 0) {
            ScriptHost.emitEvent("window.opened", localEntry);
            return;
        }
        ScriptHost.emitEvent("window.adopted",
                             { window: localEntry, previous: prev });
        if (Config.windowSync.restore)
            restore(t, prev, identity);
    }

    // Best-effort state restore. Loop prevention: the identity is
    // suppressed before we touch the compositor so the resulting
    // registry updates don't re-fire events.
    function restore(t, prev, identity) {
        sync.suppress(identity);
        try {
            if (prev.workspace && Config.compositor === "sway" && t.appId)
                I3.dispatch(`[app_id="${t.appId}"] move to workspace "${prev.workspace}"`);
            if (prev.fullscreen && !t.fullscreen)
                t.fullscreen = true;
            else if (prev.maximized && !t.maximized)
                t.maximized = true;
            if (prev.minimized)
                t.minimized = true;
            if (prev.focused)
                t.activate();
        } catch (e) {
            console.warn(`[wolfy:sync] restore failed for ${identity}: ${e}`);
        }
    }
}
