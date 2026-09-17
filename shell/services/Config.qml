pragma Singleton
import Quickshell
import Quickshell.Io

// Reads ~/.config/wolfy/config.json through a JsonAdapter with defaults,
// so every key is optional. Hot-reloads on file change.
Singleton {
    id: root

    readonly property string dir: Quickshell.env("HOME") + "/.config/wolfy"
    readonly property string configPath: dir + "/config.json"
    readonly property string scriptsDir: dir + "/scripts"

    property alias theme: adapter.theme
    property alias compositor: adapter.compositor
    property alias barPosition: adapter.barPosition
    property alias barHeight: adapter.barHeight
    property alias barOpacity: adapter.barOpacity
    property alias launcherMaxResults: adapter.launcherMaxResults
    property alias popupSeconds: adapter.popupSeconds
    property alias allowExec: adapter.allowExec
    property alias wallpaper: adapter.wallpaper
    property alias sessionCommands: adapter.sessionCommands
    property alias windowSync: adapter.windowSync

    // Raw view of the parsed config, handed to scripts as wolfy.config.
    readonly property var forScripts: ({
        theme: adapter.theme,
        compositor: adapter.compositor,
        barPosition: adapter.barPosition,
        barHeight: adapter.barHeight,
        launcherMaxResults: adapter.launcherMaxResults,
        popupSeconds: adapter.popupSeconds,
        allowExec: adapter.allowExec,
        wallpaper: adapter.wallpaper,
        windowSync: adapter.windowSync
    })

    FileView {
        id: file
        path: root.configPath
        watchChanges: true
        onFileChanged: file.reload()
        onLoadFailed: console.warn(`[wolfy] no config at ${root.configPath}; using defaults`)
        adapter: JsonAdapter {
            id: adapter
            property string theme: "moonlight"
            property string compositor: "sway"   // "sway"/"i3" or "hyprland"
            property string barPosition: "top"   // "top" | "bottom"
            property int barHeight: 34
            property real barOpacity: 0.92
            property int launcherMaxResults: 8
            property int popupSeconds: 5
            property bool allowExec: true
            property string wallpaper: ""
            property var sessionCommands: ({
                lock: "swaylock",
                logout: "swaymsg exit",
                reboot: "systemctl reboot",
                poweroff: "systemctl poweroff"
            })
            // Cross-compositor window sync. See docs/WINDOW-SYNC.md.
            property var windowSync: ({
                enabled: true,
                persist: true,     // save registry to disk across restarts
                restore: true,     // apply adopted state on window appear
                kwinBridge: false, // scripts may ensure the KWin bridge
                adoptTtlMs: 60000, // one adopt per identity per minute
                suppressMs: 4000,  // echo suppression after self-applied state
                staleMs: 600000    // expire entries unseen for 10 min
            })
        }
    }
}
