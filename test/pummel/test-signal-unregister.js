require("../common");

var childKilled = false, done = false,
    spawn = require('child_process').spawn,
    sys = require("sys"),
    child;

var join = require('path').join;

child = spawn(process.argv[0], [join(fixturesDir, 'should_exit.js')]);
child.addListener('exit', function () {
  if (!done) childKilled = true;
});

setTimeout(function () {
  sys.puts("Sending SIGINT");
  child.kill("SIGINT");
  setTimeout(function () {
    sys.puts("Chance has been given to die");
    done = true;
    if (!childKilled) {
      // Cleanup
      sys.puts("Child did not die on SIGINT, sending SIGTERM");
      child.kill("SIGTERM");
    }
  }, 200);
}, 200);

process.addListener("exit", function () {
  assert.ok(childKilled);
});
