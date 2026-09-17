pragma Singleton
import Quickshell

// Wolfy theme service. Switch with Theme.setTheme("moonlight"|"daybreak"|"forest")
// or from a script: wolfy.emit("theme.set", "forest").
Singleton {
    id: root

    property string name: "moonlight"

    readonly property var palettes: ({
        moonlight: {
            bg: "#10141c", surface: "#1a2130", overlay: "#232b3d",
            text: "#d8dee9", subtext: "#8a94a8",
            accent: "#82aaff", accent2: "#c792ea",
            warn: "#ffcb6b", err: "#ff5370", ok: "#c3e88d",
            border: "#2c3850"
        },
        daybreak: {
            bg: "#e8ecf4", surface: "#f4f6fb", overlay: "#dde3ef",
            text: "#2a3142", subtext: "#67718a",
            accent: "#3a6fd8", accent2: "#8a4fc0",
            warn: "#b07d00", err: "#c03050", ok: "#3d7d3d",
            border: "#c4cde0"
        },
        forest: {
            bg: "#0f1712", surface: "#1a251d", overlay: "#243628",
            text: "#d3e2d6", subtext: "#7f9a86",
            accent: "#7ecba1", accent2: "#e0af68",
            warn: "#e0af68", err: "#e06c75", ok: "#98c379",
            border: "#30493a"
        }
    })

    readonly property var colors: palettes[name] || palettes.moonlight

    readonly property int radius: 8
    readonly property int radiusSmall: 5
    readonly property int spacing: 8
    readonly property int fontSize: 13
    readonly property string fontFamily: "DejaVu Sans"

    function setTheme(name) {
        if (palettes[name] !== undefined)
            root.name = name;
    }

    // Convenience: next theme in the cycle, used by keybinds/scripts.
    function cycle() {
        const names = Object.keys(palettes);
        setTheme(names[(names.indexOf(root.name) + 1) % names.length]);
    }
}
