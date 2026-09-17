// Hello from Wolfy scripting.
// Runs when the shell starts; also demonstrates timers and logging.

wolfy.log("hello.js loaded, version " + wolfy.version);

wolfy.on("shell.start", function () {
    wolfy.emit("shell.notify", {
        summary: "Wolfy is running",
        body: "JavaScript scripts are live in ~/.config/wolfy/scripts",
        appName: "wolfy"
    });
});

wolfy.setTimeout(function () {
    wolfy.log("hello.js: 5-second timer fired");
}, 5000);
