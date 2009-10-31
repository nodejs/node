var path = require("path");
libDir = path.join(path.dirname(__filename), "../lib");
require.paths.unshift(libDir);
process.mixin(require("sys"));
function next (i) {
  if (i <= 0) return;

  var child = process.createChildProcess("echo", ["hello"]);

  child.addListener("output", function (chunk) {
    if (chunk) print(chunk);
  });

  child.addListener("exit", function (code) {
    if (code != 0) process.exit(-1);
    next(i - 1);
  });
}

next(500);
