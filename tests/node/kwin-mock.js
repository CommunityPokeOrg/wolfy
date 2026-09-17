// Node harness that loads kwin/wolfy-kwin-bridge.js inside a mocked
// KWin scripting environment (workspace + callDBus + print).
"use strict";

const vm = require("node:vm");
const fs = require("node:fs");
const path = require("node:path");

const BRIDGE = path.join(__dirname, "..", "..", "kwin", "wolfy-kwin-bridge.js");

function signal() {
    const fns = [];
    return {
        handlers: fns,
        connect: (fn) => fns.push(fn),
        fire: (...a) => fns.forEach(f => f(...a)),
    };
}

// kwin5: { clientAdded, clientRemoved, clientActivated, clientList() }
// kwin6: { windowAdded, windowRemoved, windowActivated, windows, activeWindow }
function makeWorkspace(kind) {
    const ws = {};
    if (kind === "kwin6") {
        ws.windowAdded = signal();
        ws.windowRemoved = signal();
        ws.windowActivated = signal();
        ws.windows = [];
        ws.activeWindow = null;
    } else {
        ws.clientAdded = signal();
        ws.clientRemoved = signal();
        ws.clientActivated = signal();
        ws._clients = [];
        ws.activeClient = null;
        ws.clientList = () => ws._clients;
    }
    return ws;
}

function makeWindow(kind, over = {}) {
    const w = {
        closed: signal(),
        minimizedChanged: signal(),
        fullScreenChanged: signal(),
        caption: "Test Window",
        minimized: false,
        fullScreen: false,
    };
    if (kind === "kwin6") {
        Object.assign(w, {
            internalId: "{id-42}",
            resourceClass: "Firefox",
            frameGeometry: { x: 10, y: 20, width: 800, height: 600 },
            desktops: [{ id: "d1", name: "2", x11DesktopNumber: 2 }],
            output: { name: "DP-1" },
            normalWindow: true,
        });
    } else {
        Object.assign(w, {
            windowId: 4242,
            resourceClass: "Firefox",
            geometry: { x: 10, y: 20, width: 800, height: 600 },
            desktop: 2,
            screen: 0,
            desktopWindow: false,
            dock: false,
            specialWindow: false,
            skipSwitcher: false,
            active: false,
        });
    }
    Object.assign(w, over);
    return w;
}

class KWinMock {
    constructor(kind = "kwin5") {
        this.kind = kind;
        this.workspace = makeWorkspace(kind);
        this.calls = []; // {service,path,iface,method,arg}
        const mock = this;
        this.sandbox = {
            workspace: this.workspace,
            callDBus: (service, p, iface, method, arg) => {
                mock.calls.push({ service, path: p, iface, method, arg });
            },
            print: () => {},
        };
        vm.createContext(this.sandbox);
        vm.runInContext(fs.readFileSync(BRIDGE, "utf8"), this.sandbox,
                        { filename: "wolfy-kwin-bridge.js" });
    }

    upserts() { return this.calls.filter(c => c.method === "upsert"); }
    removals() { return this.calls.filter(c => c.method === "remove"); }
    hellos() { return this.calls.filter(c => c.method === "hello"); }

    addWindow(win) {
        if (this.kind === "kwin6") {
            this.workspace.windows.push(win);
            this.workspace.windowAdded.fire(win);
        } else {
            this.workspace._clients.push(win);
            this.workspace.clientAdded.fire(win);
        }
    }

    removeWindow(win) {
        if (this.kind === "kwin6") {
            const i = this.workspace.windows.indexOf(win);
            if (i >= 0) this.workspace.windows.splice(i, 1);
            if (this.workspace.windowRemoved.handlers.length)
                this.workspace.windowRemoved.fire(win);
            else
                win.closed.fire();
        } else {
            const i = this.workspace._clients.indexOf(win);
            if (i >= 0) this.workspace._clients.splice(i, 1);
            if (this.workspace.clientRemoved.handlers.length)
                this.workspace.clientRemoved.fire(win);
            else
                win.closed.fire();
        }
    }
}

module.exports = { KWinMock, makeWindow, signal };
