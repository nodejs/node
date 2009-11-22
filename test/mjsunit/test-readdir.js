process.mixin(require("./common"));

var got_error = false;

var promise = posix.readdir(fixturesDir);
puts("readdir " + fixturesDir);

promise.addCallback(function (files) {
  p(files);
  assertArrayEquals(["a.js", "b", "multipart.js", "test_ca.pem", "test_cert.pem", "test_key.pem", "x.txt"], files.sort());
});

promise.addErrback(function () {
  puts("error");
  got_error = true;
});

process.addListener("exit", function () {
  assertFalse(got_error);
  puts("exit");
});
