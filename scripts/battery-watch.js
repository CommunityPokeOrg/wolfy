// Warns when the battery drops below thresholds.
// Driven by the "battery.level" events emitted by the bar's Battery
// component.

var warned20 = false;
var warned10 = false;

wolfy.on("battery.level", function (data) {
    var pct = data.percent;
    var charging = data.charging;

    if (charging) {
        warned20 = warned10 = false;   // reset once plugged in
        return;
    }
    if (pct <= 10 && !warned10) {
        warned10 = true;
        wolfy.emit("shell.notify", {
            summary: "Battery critical",
            body: pct + "% remaining — plug in now.",
            appName: "wolfy"
        });
    } else if (pct <= 20 && !warned20) {
        warned20 = true;
        wolfy.emit("shell.notify", {
            summary: "Battery low",
            body: pct + "% remaining.",
            appName: "wolfy"
        });
    }
});
