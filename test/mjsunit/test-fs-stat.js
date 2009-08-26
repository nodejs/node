include("mjsunit.js");

var got_error = false;
var got_success = false;
var stats;

var promise = node.fs.stat(".");

promise.addCallback(function (_stats) {
  stats = _stats;
  p(stats);
  got_success = true;
});

promise.addErrback(function () {
  got_error = true;
});

process.addListener("exit", function () {
  assertTrue(got_success);
  assertFalse(got_error);
  assertTrue(stats.mtime instanceof Date);
});

