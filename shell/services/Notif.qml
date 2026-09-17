pragma Singleton
import Quickshell
import Quickshell.Services.Notifications
import QtQml.Models

// Wolfy notification hub. Real notifications arrive through
// NotificationServer; scripts and internal code can post announcements
// through Notif.post(). Both end up in `model` for the popup UI.
Singleton {
    id: root

    property ListModel model: ListModel {}
    property bool doNotDisturb: false

    signal popupAdded()

    NotificationServer {
        id: server
        actionsSupported: true
        bodySupported: true
        imageSupported: true
        onNotification: function(notification) {
            notification.tracked = true;
            root.model.append({
                "source": notification,
                "summary": notification.summary,
                "body": notification.body,
                "appName": notification.appName,
                "icon": notification.appIcon,
                "synthetic": false
            });
            if (!root.doNotDisturb)
                root.popupAdded();
        }
    }

    // For scripts / internal callers: wolfy.emit("shell.notify", {...})
    // lands here via ScriptHost.
    function post(summary, body, appName, icon) {
        model.append({
            "source": null,
            "summary": summary || "",
            "body": body || "",
            "appName": appName || "wolfy",
            "icon": icon || "",
            "synthetic": true
        });
        if (!doNotDisturb)
            popupAdded();
    }

    function dismiss(index) {
        if (index < 0 || index >= model.count)
            return;
        const src = model.get(index).source;
        if (src !== null && src !== undefined)
            src.dismiss();
        model.remove(index);
    }

    function dismissAll() {
        for (let i = model.count - 1; i >= 0; --i)
            dismiss(i);
    }
}
