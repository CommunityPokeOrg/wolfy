#!/usr/bin/env bash
# Wolfy KWin bridge loader.
#
# Usage:
#   tools/kwin-bridge.sh status   # is the bridge loaded in KWin?
#   tools/kwin-bridge.sh load     # load + start the bridge script
#   tools/kwin-bridge.sh unload
#   tools/kwin-bridge.sh reload   # unload + load (full resync)
#   tools/kwin-bridge.sh ensure   # load only if not already loaded
#   tools/kwin-bridge.sh verify   # load, then check Wolfy received a hello
#   tools/kwin-bridge.sh relay    # run wolfy-sync-relay fronting KWin's bus
#
# Works over the session bus against org.kde.KWin /Scripting. Set
# DBUS_SESSION_BUS_ADDRESS for a bus other than the ambient one —
# e.g. a dbus-launch bus (see ~/.dbus/session-bus/<machine>-<display>).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BRIDGE="${KWIN_BRIDGE:-$SCRIPT_DIR/kwin/wolfy-kwin-bridge.js}"
NAME="wolfy-sync"
SERVICE="org.kde.KWin"
OBJ="/Scripting"
IFACES="org.kde.kwin.Scripting org.kde.KWin.Scripting"

QDBUS="${QDBUS:-}"
pick_qdbus() {
    for q in "${QDBUS:-}" qdbus-qt6 qdbus6 qdbus qdbus-qt5; do
        [ -n "$q" ] && command -v "$q" >/dev/null 2>&1 && { QDBUS="$q"; return; }
    done
    echo "kwin-bridge: no qdbus found" >&2; exit 1
}

iface() {
    # Discover the scripting interface name KWin exposes.
    local out
    out="$("$QDBUS" "$SERVICE" "$OBJ" 2>/dev/null || true)"
    for i in $IFACES; do
        grep -q "$i" <<<"$out" && { echo "$i"; return; }
    done
    # Fall back: interface name is accepted even without introspection.
    echo "org.kde.kwin.Scripting"
}

kwin_call() { "$QDBUS" "$SERVICE" "$OBJ" "$@" 2>/dev/null; }

cmd_status() {
    local i; i="$(iface)"
    if kwin_call "$i.isScriptLoaded" "$NAME" | grep -q true; then
        echo "loaded"
    else
        echo "not-loaded"
    fi
}

cmd_load() {
    [ -f "$BRIDGE" ] || { echo "kwin-bridge: bridge script missing: $BRIDGE" >&2; exit 1; }
    local i; i="$(iface)"
    kwin_call "$i.unloadScript" "$NAME" >/dev/null || true
    kwin_call "$i.loadScript" "$BRIDGE" "$NAME" >/dev/null
    kwin_call "$i.start" >/dev/null
    echo "kwin-bridge: loaded $BRIDGE as '$NAME' (iface $i)"
}

cmd_unload() { kwin_call "$(iface).unloadScript" "$NAME"; echo "kwin-bridge: unloaded"; }

cmd_ensure() {
    if [ "$(cmd_status)" = "loaded" ]; then
        echo "kwin-bridge: already loaded"
    else
        cmd_load
    fi
}

cmd_verify() {
    cmd_ensure >/dev/null
    sleep 1
    # If Wolfy is running, its registry must answer ping on
    # org.wolfy.WindowSync /sync (through the relay when the
    # compositors share no bus).
    local out
    out="$("$QDBUS" org.wolfy.WindowSync /sync org.wolfy.WindowSync.ping 2>/dev/null || true)"
    if grep -q true <<<"$out"; then
        echo "kwin-bridge: Wolfy answered ping; bridge up"
    else
        echo "kwin-bridge: loaded into KWin, but Wolfy did not answer ping" >&2
        echo "  (Wolfy not running? different session bus? try '$0 relay')" >&2
        exit 1
    fi
}

cmd_relay() {
    # Forward org.wolfy.WindowSync /sync between KWin's bus and Wolfy's
    # bus — needed when KWin lives on its own dbus-launch session
    # (Plasma on X11 does) while Wolfy runs on the systemd user bus.
    # KWin side: WOLFY_KWIN_BUS env, or auto-detected from
    # ~/.dbus/session-bus/<machine>-<display>. Wolfy side: ambient
    # session bus (default /run/user/$UID/bus).
    local relay="${WOLFY_RELAY:-$SCRIPT_DIR/build-qt68/core/wolfy-sync-relay}"
    [ -x "$relay" ] || relay="${WOLFY_RELAY:-$SCRIPT_DIR/build/core/wolfy-sync-relay}"
    [ -x "$relay" ] || { echo "kwin-bridge: relay not built (cmake --build <dir> --target wolfy-sync-relay)" >&2; exit 1; }
    local kbus="${WOLFY_KWIN_BUS:-}"
    if [ -z "$kbus" ]; then
        local f disp="${DISPLAY#*:}"; disp="${disp%.*}"
        f="$(ls "$HOME"/.dbus/session-bus/*-"$disp" 2>/dev/null | head -1 || true)"
        [ -n "$f" ] && kbus="$(sed -n "s/^DBUS_SESSION_BUS_ADDRESS='\(.*\)'/\1/p" "$f")"
    fi
    [ -n "$kbus" ] || { echo "kwin-bridge: cannot find KWin's bus; set WOLFY_KWIN_BUS" >&2; exit 1; }
    echo "kwin-bridge: relaying KWin bus -> session bus"
    exec "$relay" "$kbus"
}

pick_qdbus
case "${1:-status}" in
    status) cmd_status ;;
    load)   cmd_load ;;
    unload) cmd_unload ;;
    reload) cmd_unload >/dev/null 2>&1 || true; cmd_load ;;
    ensure) cmd_ensure ;;
    verify) cmd_verify ;;
    relay)  cmd_relay ;;
    *) echo "usage: $0 {status|load|unload|reload|ensure|verify|relay}" >&2; exit 2 ;;
esac
