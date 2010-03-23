var sys = require("sys"),
    childProcess = require("child_process");

function next (i) {
  if (i <= 0) return;

  var child = childProcess.spawn("echo", ["hello"]);

  child.stdout.addListener("data", function (chunk) {
    sys.print(chunk);
  });

  child.addListener("exit", function (code) {
    if (code != 0) process.exit(-1);
    next(i - 1);
  });
}

next(500);
