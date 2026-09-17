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
- **Cross-KWin window sync** — a persistent window registry fed over
  D-Bus (KWin scripting bridge included) plus the local toplevel list,
  so view/display switches adopt windows instead of re-running setup.
  See [docs/WINDOW-SYNC.md](docs/WINDOW-SYNC.md)
- **`wolfy` JavaScript API** — event bus, timers, subprocesses, config,
  window-sync access — for extending the shell without touching QML

## Repository layout

| Path | What |
|---|---|
| `shell/` | QML shell: `shell.qml` entry point, `components/` widgets, `services/` singletons |
| `core/` | `wolfycore` C++ QML plugin (`Wolfy.Core` module): the `ScriptEngine` that runs JS and the `WindowSync` registry |
| `scripts/` | Example user scripts (also the default script directory) |
| `kwin/` | `wolfy-kwin-bridge.js` — KWin scripting bridge for window sync |
| `tools/` | `kwin-bridge.sh` — load/manage the KWin bridge over D-Bus |
| `config/` | Default `config.json` |
| `tests/` | CTest unit tests (script engine + window sync), a Node.js shim suite incl. a mocked-KWin bridge harness, and `lint.sh` |
| `docs/` | Setup, architecture, scripting API, window sync, live-session docs |

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
