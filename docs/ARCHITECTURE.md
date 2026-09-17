# Wolfy architecture

```
            ┌──────────────────────────────────────────────┐
            │                 QuickShell                    │
            │  ┌────────────┐   ┌─────────────────────────┐ │
            │  │ shell.qml  │──▶│ Variants: per-screen     │ │
            │  │ (ShellRoot)│   │ Wallpaper / Bar / Popups │ │
            │  └────────────┘   └─────────────────────────┘ │
            │        │                                       │
            │   services/            components/             │
            │   Theme · Config       Bar · Launcher · Osd    │
            │   Notif · ScriptHost   SessionMenu · ...       │
            └────────┼───────────────────────────────────────┘
                     │ import Wolfy.Core
            ┌────────▼───────────────────────────────────────┐
            │ wolfycore (C++ QML plugin)                     │
            │  ScriptEngine ──▶ ScriptContext per .js file   │
            │   scriptDirs      (own QJSEngine + `wolfy` API)│
            │   autoReload      timers · exec · events       │
            └────────┬───────────────────────────────────────┘
                     │ reads
            ~/.config/wolfy/scripts/*.js
```

## Layers

**Shell layer (`shell/`)** — plain QML on QuickShell primitives.
`ShellRoot` creates one `Scope` per `ShellScreen` via `Variants`, so the
wallpaper, bar, and notification popups exist on every monitor; global
overlays (launcher, OSD, session menu) attach to the primary screen.
Surfaces use `WlrLayershell` layer rules: the bar lives on `Top` with an
exclusive zone, overlays live on `Overlay` with an empty `Region` mask
except their visible panels.

**Services (`shell/services/`)** — QML singletons:
- `Theme` — named palettes; `setTheme`/`cycle` mutate live bindings.
- `Config` — `FileView` + `JsonAdapter` over `~/.config/wolfy/config.json`,
  hot-reloaded, with defaults.
- `Notif` — a real `NotificationServer`; internal `post()` shares the model
  so script announcements and app notifications render identically.
- `ScriptHost` — owns the `ScriptEngine`, forwards well-known
  `wolfy.emit()` names (`shell.notify`, `shell.osd`, `launcher.open`,
  `session.open`, `theme.set`, `theme.cycle`, `shell.reload`, `shell.quit`)
  into shell actions, and re-raises everything else as `unhandledEvent`.

**Runtime (`core/`)** — `ScriptEngine` scans `scriptDirs` (user dir +
bundled `scripts/`), creating a `ScriptContext` per file. Each context owns
a `QJSEngine`, so script state is isolated and one bad script can't take
the shell down; syntax/runtime errors surface through `scriptError` (which
`ScriptHost` turns into a notification). `QFileSystemWatcher` + `autoReload`
re-evals scripts on save.

**Event flow** — shell → JS: components call `ScriptHost.emitEvent(name,
payload)` (e.g. `workspace.changed`, `battery.level`, `volume.changed`,
`shell.start`). JS → shell: `wolfy.emit(name, payload)` raises
`eventFromScript` → `ScriptHost.dispatch` maps known names to UI actions.

## IPC

QuickShell IPC handlers: `launcher` (toggle/open/close) and `session`
(toggle). Scripts reach the same actions via `launcher.open` /
`session.open` events.

## Testing

- `tests/test_scriptengine.cpp` (CTest/QtTest): loading, dispatch, logging,
  exec gating, timers, hot reload, config injection.
- `tests/node/` — a pure-Node `vm`-based reimplementation of the `wolfy`
  API that runs the real example scripts; catches API drift without Qt.
- `tests/lint.sh` — qmllint over all shell QML. QuickShell ships no
  qmltypes on most distros, so `tests/qml-stubs/` declares the Quickshell
  modules (and a minimal `Wolfy.Core`) in the `QtQuick.tooling` format.
