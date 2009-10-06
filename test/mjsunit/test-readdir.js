node.mixin(require("common.js"));

var got_error = false;

var promise = node.fs.readdir(fixturesDir);
puts("readdir " + fixturesDir);

promise.addCallback(function (files) {
  p(files);
  assertArrayEquals(["a.js", "b", "multipart.js", "x.txt"], files.sort());
});

promise.addErrback(function () {
  puts("error");
  got_error = true;
});

process.addListener("exit", function () {
  assertFalse(got_error);
  puts("exit");
});
