#!/usr/bin/env bash
# Wolfy installer: builds wolfycore, installs the shell under
# ~/.config/quickshell/wolfy, and seeds ~/.config/wolfy.
set -euo pipefail
cd "$(dirname "$0")"

QT_MIN="6.6.0"
BUILD_DIR="${BUILD_DIR:-build}"
QSHELL_CONFIG="${QSHELL_CONFIG:-$HOME/.config/quickshell/wolfy}"
WOLFY_CONFIG="${WOLFY_CONFIG:-$HOME/.config/wolfy}"

die() { echo "install: $*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

# --- dependency checks ---------------------------------------------------
have cmake || die "cmake not found"
have make || have ninja || die "make/ninja not found"
have quickshell || echo "install: warning: quickshell not in PATH (install it to run Wolfy)"

QT_MAJOR=""
for q in /usr/lib/qt6/bin/qmake qmake6 qmake; do
    have "$q" || [ -x "$q" ] || continue
    v="$("$q" -query QT_VERSION 2>/dev/null || true)"
    [ -n "$v" ] && { QT_MAJOR="$v"; QMAKE="$q"; break; }
done
[ -n "$QT_MAJOR" ] || die "Qt6 qmake not found (need Qt >= $QT_MIN)"
[ "$(printf '%s\n%s\n' "$QT_MIN" "$QT_MAJOR" | sort -V | head -1)" = "$QT_MIN" ] \
    || die "Qt $QT_MAJOR < required $QT_MIN"
echo "install: Qt $QT_MAJOR ($QMAKE)"

# --- build wolfycore ------------------------------------------------------
GENERATOR=()
have ninja && GENERATOR=(-G Ninja)
cmake -B "$BUILD_DIR" "${GENERATOR[@]}" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel
[ -f "$BUILD_DIR/Wolfy/Core/libwolfycore.so" ] \
    || die "build did not produce Wolfy/Core/libwolfycore.so"

# --- install shell --------------------------------------------------------
mkdir -p "$QSHELL_CONFIG" "$WOLFY_CONFIG/scripts"
cp -r shell/. "$QSHELL_CONFIG/"
cp -r "$BUILD_DIR/Wolfy" "$QSHELL_CONFIG/"
echo "install: shell -> $QSHELL_CONFIG"

[ -f "$WOLFY_CONFIG/config.json" ] \
    || cp config/config.json "$WOLFY_CONFIG/config.json"
for s in scripts/*.js; do
    base="$(basename "$s")"
    [ -f "$WOLFY_CONFIG/scripts/$base" ] || cp "$s" "$WOLFY_CONFIG/scripts/$base"
done
echo "install: config -> $WOLFY_CONFIG"

# --- validate -------------------------------------------------------------
ctest --test-dir "$BUILD_DIR" --output-on-failure
have node && node tests/node/run-tests.js || true

cat <<EOF

Wolfy installed. Run inside sway/Hyprland:

    quickshell -c wolfy

IPC:  quickshell -c wolfy ipc launcher toggle
      quickshell -c wolfy ipc session toggle
EOF
