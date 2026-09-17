// Node test shim replicating the wolfy.* API the C++ ScriptEngine
// injects into each script. Lets us unit-test user scripts without Qt.
"use strict";

const vm = require("node:vm");
const fs = require("node:fs");

class WolfyShim {
    constructor({ version = "0.1.0-test", config = {} } = {}) {
        this.handlers = new Map();   // event -> [fn]
        this.emitted = [];           // [{name, payload}]
        this.logs = [];              // [{level, msg}]
        this.timers = [];            // [{fn, ms, repeat, cleared}]
        this.execCalls = [];         // [{cmd}]
        this.config = config;

        const shim = this;
        this.api = {
            version,
            config,
            on: (name, fn) => {
                if (!shim.handlers.has(name)) shim.handlers.set(name, []);
                shim.handlers.get(name).push(fn);
            },
            off: (name) => shim.handlers.delete(name),
            emit: (name, payload) => shim.emitted.push({ name, payload }),
            log: (...a) => shim.logs.push({ level: "log", msg: a.join(" ") }),
            warn: (...a) => shim.logs.push({ level: "warn", msg: a.join(" ") }),
            error: (...a) => shim.logs.push({ level: "error", msg: a.join(" ") }),
            setTimeout: (fn, ms) => {
                shim.timers.push({ fn, ms, repeat: false, cleared: false });
                return shim.timers.length;
            },
            setInterval: (fn, ms) => {
                shim.timers.push({ fn, ms, repeat: true, cleared: false });
                return shim.timers.length;
            },
            clearTimer: (id) => { if (shim.timers[id - 1]) shim.timers[id - 1].cleared = true; },
            exec: (cmd, cb) => {
                shim.execCalls.push({ cmd });
                if (cb) cb("shim-output", 0, "");
            },
            env: (name) => process.env[name] || "",
            quit: () => { throw new Error("wolfy.quit()"); },
        };
    }

    load(path) {
        const src = fs.readFileSync(path, "utf8");
        const sandbox = { wolfy: this.api, console, Date, Math };
        vm.createContext(sandbox);
        vm.runInContext(src, sandbox, { filename: path });
        return sandbox;
    }

    fire(name, payload) {
        for (const fn of this.handlers.get(name) || []) fn(payload);
    }

    runTimers() {
        for (const t of this.timers) if (!t.cleared) t.fn();
    }
}

module.exports = { WolfyShim };
