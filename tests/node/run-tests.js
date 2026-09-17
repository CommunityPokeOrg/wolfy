// Validates every bundled script under the wolfy API shim.
// Run: node tests/node/run-tests.js
"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const path = require("node:path");
const fs = require("node:fs");
const { WolfyShim } = require("./wolfy-shim.js");

const SCRIPTS = path.join(__dirname, "..", "..", "scripts");
const files = fs.readdirSync(SCRIPTS).filter(f => f.endsWith(".js"));

test("all bundled scripts load without throwing", () => {
    for (const f of files) {
        const shim = new WolfyShim();
        assert.doesNotThrow(() => shim.load(path.join(SCRIPTS, f)), f);
    }
});

test("hello.js announces itself on shell.start", () => {
    const shim = new WolfyShim();
    shim.load(path.join(SCRIPTS, "hello.js"));
    shim.fire("shell.start");
    const note = shim.emitted.find(e => e.name === "shell.notify");
    assert.ok(note, "expected shell.notify");
    assert.match(note.payload.summary, /Wolfy/i);
});

test("battery-watch.js warns once at <=20 and escalates at <=10", () => {
    const shim = new WolfyShim();
    shim.load(path.join(SCRIPTS, "battery-watch.js"));
    const warns = () => shim.emitted.filter(e => e.name === "shell.notify");

    shim.fire("battery.level", { percent: 25, charging: false });
    assert.equal(warns().length, 0);
    shim.fire("battery.level", { percent: 18, charging: false });
    assert.equal(warns().length, 1);
    shim.fire("battery.level", { percent: 15, charging: false });
    assert.equal(warns().length, 1);                 // no repeat at same band
    shim.fire("battery.level", { percent: 8, charging: false });
    assert.equal(warns().length, 2);
    shim.fire("battery.level", { percent: 60, charging: true });
    shim.fire("battery.level", { percent: 9, charging: false });
    assert.equal(warns().length, 3);                 // reset after charging
});

test("auto-theme.js picks daybreak by day and moonlight by night", () => {
    const shim = new WolfyShim();
    shim.load(path.join(SCRIPTS, "auto-theme.js"));
    const hour = new Date().getHours();
    shim.fire("shell.start");
    const set = shim.emitted.find(e => e.name === "theme.set");
    assert.ok(set, "expected theme.set");
    const want = hour >= 7 && hour < 19 ? "daybreak" : "moonlight";
    assert.equal(set.payload, want);
});

test("volume-osd.js forwards volume.changed to shell.osd", () => {
    const shim = new WolfyShim();
    shim.load(path.join(SCRIPTS, "volume-osd.js"));
    shim.fire("volume.changed", { percent: 40, muted: false });
    const osd = shim.emitted.find(e => e.name === "shell.osd");
    assert.ok(osd);
    assert.equal(osd.payload.value, 40);
    shim.fire("volume.changed", { percent: 40, muted: true });
    assert.equal(shim.emitted.filter(e => e.name === "shell.osd").at(-1).payload.value, 0);
});

test("workspace-log.js logs workspace events and calls exec", () => {
    const shim = new WolfyShim();
    shim.load(path.join(SCRIPTS, "workspace-log.js"));
    assert.equal(shim.execCalls.length, 1);
    assert.match(shim.execCalls[0].cmd, /uname/);
    shim.fire("workspace.changed", { name: "3" });
    assert.ok(shim.logs.some(l => /workspace changed -> 3/.test(l.msg)));
});
