// Time-based theme switching: daybreak 07:00-19:00, moonlight otherwise.
// Checks at startup and then every 10 minutes.

function themeForNow() {
    var h = new Date().getHours();
    return (h >= 7 && h < 19) ? "daybreak" : "moonlight";
}

function apply() {
    var want = themeForNow();
    wolfy.log("auto-theme: time suggests " + want);
    wolfy.emit("theme.set", want);
}

wolfy.on("shell.start", apply);
wolfy.setInterval(apply, 10 * 60 * 1000);
