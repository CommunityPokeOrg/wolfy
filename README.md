# Wolfy

**Wolfy** is a custom desktop environment built on
[QuickShell](https://quickshell.outfoxxed.me/) — the Qt6/QML Wayland shell
framework — with a first-class **JavaScript scripting runtime** for
customization and extensibility.

Wolfy runs as a layer-shell surface set on wlroots compositors (sway,
Hyprland) and provides:

- **Top/bottom bar** with workspaces, clock, battery, volume, system tray,
  notification bell, and a session button
- **App launcher** overlay (fuzzy-filtered `DesktopEntries`, keyboard-driven)
- **Notification server + popups** with do-not-disturb
- **OSD** (volume, custom script events)
- **Session menu** (lock / logout / reboot / poweroff, configurable commands)
- **Theme engine** (moonlight / daybreak / forest palettes, live-switchable)
- **JSON config** at `~/.config/wolfy/config.json`, hot-reloaded
- **`wolfy` JavaScript API** — event bus, timers, subprocesses, config —
  for extending the shell without touching QML

## Repository layout

| Path | What |
|---|---|
| `shell/` | QML shell: `shell.qml` entry point, `components/` widgets, `services/` singletons |
| `core/` | `wolfycore` C++ QML plugin (`Wolfy.Core` module): the `ScriptEngine` that runs JS |
| `scripts/` | Example user scripts (also the default script directory) |
| `config/` | Default `config.json` |
| `tests/` | CTest unit tests for the script engine, a Node.js shim test suite, and `lint.sh` (qmllint + bundled qmltypes stubs) |
| `docs/` | Setup, architecture, scripting API, live-session docs |

## Quick start

```sh
./install.sh            # checks deps, builds wolfycore, installs shell+config
quickshell -c wolfy     # run inside sway or Hyprland
```

See **[docs/SETUP.md](docs/SETUP.md)** for the full dependency list and
build instructions, **[docs/SCRIPTING.md](docs/SCRIPTING.md)** for the
`wolfy.*` JavaScript API, and **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)**
for how the pieces fit together.

## Example script

```js
// ~/.config/wolfy/scripts/hello.js
wolfy.on("shell.start", () => {
  wolfy.emit("shell.notify", { summary: "Wolfy", body: "scripts online" });
});
wolfy.on("battery.level", (p) => {
  if (p.percent < 15) wolfy.emit("shell.notify",
    { summary: "Low battery", body: p.percent + "%" });
});
```

## Validation

```sh
cmake -B build && cmake --build build   # builds wolfycore
ctest --test-dir build                  # script engine unit tests
node tests/node/run-tests.js            # JS API shim tests (no Qt needed)
bash tests/lint.sh                      # qmllint over all shell QML
```

License: MIT (see `LICENSE`).
