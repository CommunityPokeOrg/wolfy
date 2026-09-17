import Quickshell
import "services"
import "components"

// Wolfy shell entry point. Per-screen surfaces are built through
// Variants; overlays that only make sense once live on the primary
// screen.
ShellRoot {
    id: root

    // Touch the singletons so the script runtime and window registry
    // start with the shell.
    property var scriptsReady: ScriptHost.engine
    property var syncReady: WinSync.bridgeConnected

    Variants {
        model: Quickshell.screens
        delegate: Scope {
            required property ShellScreen modelData

            Wallpaper { screen: modelData }
            Bar { screen: modelData }
            NotificationPopups { screen: modelData }
        }
    }

    Launcher { screen: Quickshell.screens[0] }
    Osd { screen: Quickshell.screens[0] }
    SessionMenu { screen: Quickshell.screens[0] }
}
