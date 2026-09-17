# Wolfy scripting API

Drop `.js` files in `~/.config/wolfy/scripts/` (or the repo's `scripts/`
dir for bundled examples). Each file runs in its own JS engine with the
global `wolfy` object. Scripts hot-reload on save.

## `wolfy.on(name, handler)` / `wolfy.off(name, handler)`

Subscribe to shell events. `handler(payload)` receives a plain object.

| Event | Payload | Emitted when |
|---|---|---|
| `shell.start` | `{ loaded: n }` | all scripts loaded |
| `workspace.changed` | `{ name }` | focused workspace changed |
| `battery.level` | `{ percent, charging }` | battery level changes |
| `volume.changed` | `{ percent, muted }` | sink volume/mute changes |
| custom | any | `ScriptHost.emitEvent(...)` from QML |

## `wolfy.emit(name, payload)`

Send an event into the shell. Well-known names:

| Event | Payload | Effect |
|---|---|---|
| `shell.notify` | `{ summary, body?, appName?, icon? }` | posts a notification |
| `shell.osd` | `{ icon?, label, value? }` | flashes the OSD |
| `launcher.open` | — | opens the launcher |
| `session.open` | — | opens the session menu |
| `theme.set` | `"moonlight"\|"daybreak"\|"forest"` | switches theme |
| `theme.cycle` | — | next theme |
| `shell.reload` | — | restarts the shell |
| `shell.quit` | — | exits QuickShell |

Anything else reaches `ScriptHost.unhandledEvent` for QML code to consume.

## Timers

`wolfy.setTimeout(fn, ms)` → id · `wolfy.setInterval(fn, ms)` → id ·
`wolfy.clearTimer(id)`

## `wolfy.exec(command)`

Runs `command` via `/bin/sh -c`. Returns `{ ok, stdout, stderr, code }`.
**Gated by `allowExec` in config** — set `"allowExec": false` to disable
entirely (calls then fail with `{ ok: false }`).

## `wolfy.env(name)`

Reads an environment variable (string or `undefined`).

## `wolfy.log(msg)` / `wolfy.warn(msg)` / `wolfy.error(msg)`

Logged to the QuickShell console, tagged with the script name.

## `wolfy.config`

Snapshot of the config the shell loaded (same keys as `config.json`).

## `wolfy.quit()`

Terminates this script's context.

## Bundled examples

- `hello.js` — startup notification
- `battery-watch.js` — low-battery alert via `battery.level`
- `volume-osd.js` — custom OSD text on `volume.changed`
- `workspace-log.js` — logs workspace switches
- `auto-theme.js` — `setInterval` theme cycling
