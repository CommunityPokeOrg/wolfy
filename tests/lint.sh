#!/usr/bin/env bash
# Lints all Wolfy QML files with qmllint against the bundled
# Quickshell/Wolfy qmltypes stubs, plus the real QtQuick imports.
#
#   tests/lint.sh [shell-dir]
#
# Exit 0 = clean (or only known-stub-limitation warnings), 1 = real findings.
set -u
cd "$(dirname "$0")/.."
SHELL_DIR="${1:-shell}"
STUBS="tests/qml-stubs"

# Prefer explicit Qt6 binaries: bare "qmllint" is often a qtchooser shim
# that defaults to a (possibly absent) Qt5 installation.
QMLLINT="${QMLLINT:-}"
if [ -z "$QMLLINT" ]; then
    for c in /usr/lib/qt6/bin/qmllint qmllint6 qmllint; do
        command -v "$c" >/dev/null 2>&1 && { QMLLINT="$c"; break; }
    done
fi
[ -n "$QMLLINT" ] || { echo "qmllint not found"; exit 1; }

# Qt import paths so real modules (QtQuick, QtQuick.Controls, ...) resolve.
QT_IMPORTS="$(dirname "$(dirname "$QMLLINT")")/qml"
[ -d "$QT_IMPORTS" ] || QT_IMPORTS=/usr/lib/x86_64-linux-gnu/qt6/qml

# --unqualified disable: delegate modelData/index access is idiomatic QML;
# qmllint 6.2 cannot see through Repeater/ListModel delegate scopes.
LINT_ARGS=(--unqualified disable)

# Warning texts produced only by stub limitations on this Qt version, not by
# real issues in our code. Filtered before deciding pass/fail.
KNOWN_NOISE='No type found for property "acceptedButtons"|Type of property "childrenRect" not found'

FAILED=0
while IFS= read -r -d '' f; do
    raw="$("$QMLLINT" "${LINT_ARGS[@]}" -I "$STUBS" -I "$QT_IMPORTS" -I "$SHELL_DIR" "$f" 2>&1)"
    # Drop warning blocks whose message line matches KNOWN_NOISE. A block runs
    # from its "Warning:" line to the next bare "---" separator.
    out="$(printf '%s\n' "$raw" | awk -v pat="$KNOWN_NOISE" '
        /^Warning:/ { skip = ($0 ~ pat) }
        /^---$/ { if (skip) { skip = 0; next } }
        !skip { print }
    ')"
    if [ -n "$(printf '%s' "$out" | grep -v '^--*$' | grep -v '^$')" ]; then
        echo "=== $f ==="
        printf '%s\n' "$out"
        FAILED=1
    fi
done < <(find "$SHELL_DIR" -name '*.qml' -print0 | sort -z)

if [ $FAILED -eq 0 ]; then
    echo "lint: all files clean"
else
    echo "lint: FAILURES"
fi
exit $FAILED
