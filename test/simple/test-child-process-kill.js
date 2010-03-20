require("../common");

var spawn = require('child_process').spawn;

var exitStatus = -1;
var gotStdoutEOF = false;
var gotStderrEOF = false;

var cat = spawn("cat");


cat.stdout.addListener("data", function (chunk) {
  assert.ok(false);
});

cat.stdout.addListener("end", function () {
  gotStdoutEOF = true;
});

cat.stderr.addListener("data", function (chunk) {
  assert.ok(false);
});

cat.stderr.addListener("end", function () {
  gotStderrEOF = true;
});

cat.addListener("exit", function (status) {
  exitStatus = status;
});

cat.kill();

process.addListener("exit", function () {
  assert.ok(exitStatus > 0);
  assert.ok(gotStdoutEOF);
  assert.ok(gotStderrEOF);
});
