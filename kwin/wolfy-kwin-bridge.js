// Wolfy <-> KWin window sync bridge.
//
// Loaded into KWin via tools/kwin-bridge.sh (qdbus org.kde.KWin
// /Scripting loadScript + start). Serializes every window event into
// QVariantMap-shaped objects and pushes them to Wolfy's session D-Bus
// service org.wolfy.WindowSync at /sync.
//
// Compatible with both scripting APIs:
//   KWin5 (Plasma 5.x): workspace.clientAdded/clientRemoved, Client
//                       exposes windowId/resourceClass/caption/geometry/
//                       desktop/screen/minimized/fullScreen/active.
//   KWin6 (Plasma 6.x): workspace.windowAdded, Window exposes
//                       internalId/resourceClass/caption/frameGeometry/
//                       desktops/output/minimized/fullScreen.
//
// Recovery: KWin scripts have no timers, so the bridge retries its
// hello() handshake on every window event until Wolfy answers; the
// loader (kwin-bridge.sh ensure) can re-run start for a full resync.
var WOLFY_SERVICE = "org.wolfy.WindowSync";
var WOLFY_PATH = "/sync";
// The exported interface name comes from Q_CLASSINFO; try likely
// candidates so the bridge survives renames.
var WOLFY_IFACES = ["org.wolfy.WindowSync", "WindowSync"];

var connected = false;
var iface = WOLFY_IFACES[0];
var idCounter = 0;
var syntheticIds = {}; // weak fallback for windows with no stable id

function send(method, arg) {
    try {
        callDBus(WOLFY_SERVICE, WOLFY_PATH, iface, method, arg);
        return true;
    } catch (e) {
        // Try remaining interface names once, then give up quietly —
        // Wolfy may simply not be running.
        for (var i = 0; i < WOLFY_IFACES.length; i++) {
            if (WOLFY_IFACES[i] === iface) continue;
            try {
                callDBus(WOLFY_SERVICE, WOLFY_PATH, WOLFY_IFACES[i], method, arg);
                iface = WOLFY_IFACES[i];
                return true;
            } catch (e2) {}
        }
        return false;
    }
}

function hello() {
    connected = send("hello", "kwin");
    return connected;
}

function isKWin6() {
    return typeof workspace.windowAdded !== "undefined";
}

function windowList() {
    // KWin6: workspace.windows; KWin5: workspace.clientList()
    if (typeof workspace.windows !== "undefined") return workspace.windows;
    if (typeof workspace.clientList === "function") return workspace.clientList();
    return [];
}

function winId(w) {
    if (w.internalId !== undefined && w.internalId !== null) return String(w.internalId);
    if (w.windowId !== undefined) return "x" + String(w.windowId);
    if (!syntheticIds[w]) syntheticIds[w] = "k" + (++idCounter);
    return syntheticIds[w];
}

function appId(w) {
    if (w.resourceClass !== undefined && w.resourceClass !== null)
        return String(w.resourceClass);
    if (w.desktopFileName) return String(w.desktopFileName);
    return "";
}

function geo(w) {
    var g = w.frameGeometry || w.geometry || { x: 0, y: 0, width: 0, height: 0 };
    return { x: g.x, y: g.y, width: g.width, height: g.height };
}

function workspaceName(w) {
    // KWin6: w.desktops -> [VirtualDesktop{id,x11DesktopNumber,name}]
    if (w.desktops && w.desktops.length !== undefined) {
        if (w.desktops.length === 0) return "all";
        var d = w.desktops[0];
        return String(d.name || d.x11DesktopNumber || d.id || "");
    }
    // KWin5: w.desktop -> int (-1 = on all desktops)
    if (w.desktop !== undefined) return w.desktop === -1 ? "all" : String(w.desktop);
    return "";
}

function outputName(w) {
    if (w.output && w.output.name) return String(w.output.name);
    if (w.screen !== undefined) return "screen" + w.screen;
    return "";
}

function maximized(w) {
    if (w.maximized !== undefined) return !!w.maximized;
    if (w.maximizedVertically !== undefined && w.maximizedHorizontally !== undefined)
        return !!(w.maximizedVertically && w.maximizedHorizontally);
    return false;
}

function isWindow(w) {
    // Skip desktop windows, docks, notifications, tooltips etc.
    if (w.desktopWindow || w.dock || w.notification || w.splash ||
        w.tooltip || w.menu || w.deleted)
        return false;
    if (w.normalWindow !== undefined && !w.normalWindow)
        return false;
    if (w.specialWindow) return false;
    if (w.skipSwitcher) return false;
    return true;
}

function activeWindow() {
    return (typeof workspace.activeWindow !== "undefined") ? workspace.activeWindow
         : (typeof workspace.activeClient !== "undefined") ? workspace.activeClient
         : null;
}

function serialize(w) {
    return {
        source: "kwin",
        id: winId(w),
        appId: appId(w),
        title: String(w.caption !== undefined ? w.caption : ""),
        rect: geo(w),
        workspace: workspaceName(w),
        output: outputName(w),
        minimized: !!w.minimized,
        maximized: maximized(w),
        fullscreen: !!w.fullScreen,
        focused: w === activeWindow()
    };
}

function scrub(o) {
    // null/undefined values marshal as QMetaType::Nullptr, which the
    // registry (and libdbus on its side) cannot carry — drop them.
    var out = {};
    for (var k in o) {
        var v = o[k];
        if (v === null || v === undefined) continue;
        out[k] = v;
    }
    return out;
}

function push(w) {
    if (!isWindow(w)) return;
    send("upsert", scrub(serialize(w)));
}

function drop(w) {
    send("remove", "kwin|" + winId(w));
}

function syncAll() {
    var list = windowList();
    for (var i = 0; i < list.length; i++)
        push(list[i]);
}

function hookWindow(w) {
    if (!isWindow(w)) return;
    var refresh = function() { push(w); };
    // Per-window property signals; names differ across versions so
    // hook whatever exists.
    var signals = ["minimizedChanged", "fullScreenChanged", "activeChanged",
                   "desktopsChanged", "desktopChanged", "outputChanged",
                   "screenChanged", "frameGeometryChanged", "geometryChanged",
                   "captionChanged", "maximizedChanged"];
    for (var i = 0; i < signals.length; i++) {
        var sig = signals[i];
        if (w[sig] && typeof w[sig].connect === "function") {
            try { w[sig].connect(refresh); } catch (e) {}
        }
    }
    if (w.closed && typeof w.closed.connect === "function") {
        try { w.closed.connect(function() { drop(w); }); } catch (e) {}
    }
    push(w);
}

function trackActivity() {
    // Re-push focus state when the active window changes.
    var sig = (typeof workspace.windowActivated !== "undefined") ? workspace.windowActivated
            : (typeof workspace.clientActivated !== "undefined") ? workspace.clientActivated
            : null;
    if (sig && typeof sig.connect === "function") {
        try {
            sig.connect(function(w) {
                if (w) push(w);
            });
        } catch (e) {}
    }
}

// --- bootstrap -------------------------------------------------------
if (isKWin6()) {
    workspace.windowAdded.connect(hookWindow);
    if (workspace.windowRemoved) {
        try { workspace.windowRemoved.connect(function(w) { drop(w); }); } catch (e) {}
    }
} else {
    workspace.clientAdded.connect(hookWindow);
    if (workspace.clientRemoved) {
        try { workspace.clientRemoved.connect(function(w) { drop(w); }); } catch (e) {}
    }
}
trackActivity();
hello();
syncAll();
