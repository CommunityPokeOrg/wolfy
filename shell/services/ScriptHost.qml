pragma Singleton
import Quickshell
import Wolfy.Core 1.0
import QtQml
import QtQuick

// ScriptHost owns the JavaScript runtime. It points the ScriptEngine at
// the user scripts dir (~/.config/wolfy/scripts) plus the bundled
// examples, feeds config in as wolfy.config, and translates well-known
// wolfy.emit() events into shell actions.
Singleton {
    id: root

    // Bundled scripts ship inside the repo; shellDir points at shell/.
    readonly property string bundledScriptsDir: Quickshell.shellDir + "/../scripts"

    // Signals the UI layer binds to.
    signal launcherRequested()
    signal sessionMenuRequested()
    signal osdRequested(var payload)

    ScriptEngine {
        id: engine
        scriptDirs: [Config.scriptsDir, root.bundledScriptsDir]
        autoReload: true
        allowExec: Config.allowExec

        onEventFromScript: function(name, payload) { root.dispatch(name, payload) }
        onLogMessage: function(level, msg, src) {
            console.log(`[wolfy:${src}] ${msg}`);
        }
        onScriptLoaded: function(path) {
            console.log(`[wolfy] loaded ${path}`);
        }
        onScriptError: function(path, msg, line) {
            console.error(`[wolfy] ${path}:${line} ${msg}`);
            Notif.post("Script error", `${path.split("/").pop()}:${line} ${msg}`,
                       "wolfy");
        }
    }

    readonly property alias engine: engine
    readonly property var scripts: engine.scripts

    // QML -> JS. Everything interesting in the shell funnels through here.
    function emitEvent(name, payload) {
        engine.emitEvent(name, payload === undefined ? null : payload);
    }

    // JS -> QML. Well-known event names become shell actions; anything
    // else is forwarded to subscribers via `unhandledEvent`.
    signal unhandledEvent(string name, var payload)
    function dispatch(name, payload) {
        const p = payload === null ? {} : payload;
        switch (name) {
        case "shell.notify":
            Notif.post(p.summary, p.body, p.appName, p.icon);
            break;
        case "shell.osd":
            root.osdRequested(p);
            break;
        case "launcher.open":
            root.launcherRequested();
            break;
        case "session.open":
            root.sessionMenuRequested();
            break;
        case "theme.set":
            Theme.setTheme(String(p.value || p));
            break;
        case "theme.cycle":
            Theme.cycle();
            break;
        case "shell.reload":
            Quickshell.execDetached(["quickshell", "-c", "wolfy"]);
            break;
        case "shell.quit":
            Qt.quit();
            break;
        default:
            root.unhandledEvent(name, p);
        }
    }

    Component.onCompleted: {
        engine.setConfigObject(Config.forScripts);
        emitEvent("shell.start", { loaded: engine.loadedCount });
    }
}
