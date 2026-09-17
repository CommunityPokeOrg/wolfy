// Demonstrates workspace events and wolfy.exec().

wolfy.on("workspace.changed", function (data) {
    wolfy.log("workspace changed -> " + data.name);
});

// Example of exec: log the kernel version once at load.
wolfy.exec("uname -sr", function (out, code) {
    wolfy.log("running on " + out + " (exit " + code + ")");
});
