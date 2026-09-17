# Wolfy setup & build guide

## Dependencies

| Component | Version | Notes |
|---|---|---|
| Qt6 (Core, Qml, Quick, Test) | >= 6.6 required, 6.8 recommended | QuickShell needs >= 6.6 |
| QuickShell | latest | https://quickshell.outfoxxed.me — the shell framework Wolfy runs on |
| CMake | >= 3.20 | |
| C++17 compiler | gcc >= 11 / clang >= 14 | |
| Node.js | any | only for `tests/node/run-tests.js` |
| sway or Hyprland | | host compositor |

Optional (enables extra widgets): `Pipewire` (volume), `UPower` (battery),
system-tray host support in the compositor.

### Ubuntu/Debian packages

```sh
sudo apt install build-essential cmake qt6-base-dev qt6-declarative-dev \
                 qml6-module-qtquick qt6-base-dev-tools nodejs qdbus
```

QuickShell itself is built from source (see below) or installed from your
distro/AUR (`quickshell-git` on Arch).

## Building wolfycore (the JS runtime plugin)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This produces `build/Wolfy/Core/libwolfycore.so` + `qmldir`/`*.qmltypes` —
a QML plugin module named `Wolfy.Core`.

## Installing the shell

`./install.sh` does all of this:

1. Verifies `cmake`, a Qt6 toolchain (>= 6.6), and `quickshell` are present.
2. Builds `wolfycore` into `build/`.
3. Copies the shell to `~/.config/quickshell/wolfy/` — QuickShell's config
   name is `wolfy` (`quickshell -c wolfy` resolves there).
4. Copies `build/Wolfy/` into `~/.config/quickshell/wolfy/` so `import
   Wolfy.Core` resolves (the shell directory is an implicit import path).
5. Installs the default config to `~/.config/wolfy/config.json` (skipped if
   one exists) and creates `~/.config/wolfy/scripts/` seeded with the
   example scripts.

## Running

Inside sway or Hyprland:

```sh
quickshell -c wolfy
```

Handy live controls:

```sh
quickshell -c wolfy ipc launcher toggle   # open/close the app launcher
quickshell -c wolfy ipc session toggle    # session menu
```

## Validation

```sh
ctest --test-dir build          # C++ unit tests for ScriptEngine
node tests/node/run-tests.js    # JS API contract tests (Node shim)
bash tests/lint.sh              # qmllint on all shell QML (uses bundled stubs)
```

`tests/lint.sh` needs `qmllint` (Qt6). It lints against the stub qmltypes in
`tests/qml-stubs/` so QuickShell does not have to be installed to lint.

## Troubleshooting

- **`module "Wolfy.Core" is not installed`** — `Wolfy/` didn't land next to
  `shell.qml`; re-run `./install.sh` or copy `build/Wolfy` into
  `~/.config/quickshell/wolfy/`.
- **Scripts not loading** — check the QuickShell console output; script
  errors are logged and also posted as notifications.
- **Volume shows `--`** — the Pipewire service module is unavailable;
  everything else still works.
