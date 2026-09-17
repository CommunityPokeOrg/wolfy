# Wolfy Window Sync (cross-KWin / multi-view window state)

Wolfy can track window state across displays, views, and shell sessions so
that switching views or restarting the shell does not treat already-known
windows as brand new — no re-running per-window setup, no duplicate
launchers.

## Architecture

```
KWin (host session)                Wolfy / QuickShell (nested or native)
┌──────────────────┐   session     ┌───────────────────────────────┐
│ wolfy-kwin-      │   D-Bus       │  WindowSync (C++, wolfycore)  │
│   bridge.js      │──────────────▶│  org.wolfy.WindowSync /sync   │
│  (KWin script)   │  upsert/      │                               │
└──────────────────┘  remove/      │   registry + tombstones +     │
                     hello         │   adopt + suppress + persist  │
local toplevels ──────────────▶    │          │                    │
(wlr-foreign-                      │          ▼                    │
 toplevel-management)              │   WinSync.qml singleton       │
                                   │   adopt-on-appear + restore   │
                                   │          │                    │
                                   │          ▼                    │
                                   │   window.* script events      │
                                   └───────────────────────────────┘
```

- **Sources** push entries: `<source>|<id>` keys (`kwin|…`, `local|…`).
  A source can be anything that can call D-Bus (the KWin bridge, another
  Wolfy instance, a script) or QML (`WinSync` feeds the local compositor's
  wlr foreign-toplevel list).
- **Registry** stores, per window: `appId`/`identity`, `title`, `rect`,
  `workspace`, `output`, `minimized`, `maximized`, `fullscreen`,
  `focused`, `lastSeen`, `gone`, `adopted`.
- **Persistence**: debounced JSON write to
  `~/.config/wolfy/window-sync.json` (mode 0600). On restart, entries
  load as *unconfirmed tombstones* (`gone: true`) — adoptable, and
  re-activated only if the window is seen again.

## Adoption semantics — the "don't re-run" guarantee

When a toplevel appears, `WinSync`:

1. upserts it as `local|<n>`;
2. calls `sync.adopt(identity, excludeKey)` — the freshest entry for the
   same app identity *other than itself*: a live KWin entry, another
   view's entry, or a tombstone from persistence;
3. on a hit emits `window.adopted { window, previous }` and applies
   `restore` (workspace move on sway/i3, minimized/maximized/fullscreen,
   focus);
4. on a miss emits `window.opened` — the only event that should trigger
   first-open behaviour in scripts.

An identity can only be adopted **once per `adoptTtlMs`** (default 60s),
so a window bouncing between views can't re-adopt in a loop.

## Loop prevention

- `suppress(identity)` before applying compositor commands silences
  `windowAdded/Updated/Removed` for `suppressMs` (default 4s) — state we
  set ourselves doesn't echo back as events.
- Adoption claims are atomic (`adopt()` returns `{}` on the second call
  within the TTL), so two shells racing to adopt the same window lose
  deterministically, not flappily.
- Tombstones expire (`adoptGraceMs` 5min); registry entries expire after
  `staleMs` (default 10min) via the sweeper.

## Reconnect / recovery

- Sources say `hello(source)` on connect → `bridgeConnected` flips true,
  they receive a `snapshot()` to reconcile against; `bye(source)` or
  `staleMs` of silence flips it back.
- The KWin bridge retries `hello` on every window event until it gets
  through, and re-pushes its full window list on (re)connect.
- `tools/kwin-bridge.sh ensure|reload` re-drives the KWin side from the
  shell; the bundled `window-sync.js` can run it when
  `windowSync.kwinBridge` is enabled.
- `registerService()` re-announces the D-Bus object after a bus hiccup.

## KWin bridge

`kwin/wolfy-kwin-bridge.js` is loaded into the **host** KWin (works on a
plain X11 Plasma session — no Wayland needed on the KWin side):

```bash
tools/kwin-bridge.sh status|load|unload|reload|ensure|verify
```

It supports both scripting APIs — Plasma 5 (`workspace.clientAdded`,
`Client.geometry`, `desktop`, `screen`) and Plasma 6
(`workspace.windowAdded`, `Window.frameGeometry`, `desktops`, `output`)
— and skips non-normal windows (docks, desktop windows, tooltips,
skip-switcher).

Wire-up: `org.kde.KWin /Scripting` `loadScript` + `start` over the
session bus, found via `qdbus`/`qdbus-qt6`. On Plasma 6 the interface
name may be `org.kde.KWin.Scripting`; the loader probes both.

### When KWin and Wolfy share no bus

Plasma-on-X11 sessions often run on their own dbus-launch bus
(`unix:abstract=/tmp/dbus-…`, recorded in
`~/.dbus/session-bus/<machine>-<display>`) while Wolfy runs on the
systemd user bus (`/run/user/$UID/bus`). The bridge's `callDBus` only
reaches *KWin's* bus, so `tools/sync-relay.cpp` ships
`wolfy-sync-relay`: a `QDBusVirtualObject` that owns
`org.wolfy.WindowSync`/`/sync` on the KWin bus and replays every method
call onto Wolfy's bus, translating replies (reply serials are
regenerated per-bus — passing the raw reply through never matches the
caller). Run it via:

```bash
tools/kwin-bridge.sh relay     # auto-detects KWin's bus for $DISPLAY
# or: WOLFY_KWIN_BUS=unix:abstract=… WOLFY_RELAY=path/to/wolfy-sync-relay tools/kwin-bridge.sh relay
```

## Security

- The sync service lives on the **session** bus only — per-user, no
  network exposure, no cross-user access.
- `window-sync.json` is written 0600 and only read back by the same
  user.
- The KWin bridge can only *report* state; Wolfy never lets a remote
  source execute commands — adoption applies compositor actions under
  the shell's own control and `suppress()` covers the echo.
- `windowSync.enabled: false` turns the whole feature off (no D-Bus
  service, no tracking).

## Limitations

- **Windows cannot be migrated between compositors.** Sync shares
  *state*; adoption restores what the target compositor's protocol
  allows. A window that "moves displays" is actually closed on one and
  reopened on the other — adoption makes that invisible to scripts.
- Geometry restore is best-effort: wlr foreign-toplevel has no
  request-geometry for regular windows, so `rect` is synced for
  awareness, not applied under sway. i3/sway geometry restore could be
  added via per-app rules — deliberately left out.
- Workspace restore only implemented for sway/i3 (`I3.dispatch`).
- App identity is `appId`/`resourceClass` normalized (lowercased,
  `.desktop` stripped). XWayland clients and Flatpak app-ids usually
  match; browsers that share one app-id across windows can't be told
  apart — adoption picks the *freshest* entry.
- Per-application state (document contents, scroll position, terminal
  scrollback) is out of scope — the app owns that.
- `focused` from KWin means "was active at last event"; multi-seat is
  untested.

## Configuration

`~/.config/wolfy/config.json`:

```json
"windowSync": {
    "enabled": true,
    "persist": true,
    "restore": true,
    "kwinBridge": false,
    "adoptTtlMs": 60000,
    "suppressMs": 4000,
    "staleMs": 600000
}
```

## Script API

- `wolfy.sync.windows()` — all registry entries
- `wolfy.sync.find(identity)` — freshest live entry for an app id
- `wolfy.sync.adopt(identity)` — claim (returns `{}` if already claimed)
- `wolfy.sync.upsert(entry)` / `wolfy.sync.remove(key)` — act as a source
- `wolfy.sync.suppress(identity, ms)` — echo suppression
- Events: `window.opened`, `window.adopted`, `window.added`,
  `window.updated`, `window.closed`, `sync.connected`

See `scripts/window-sync.js` for a working example.
