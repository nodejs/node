process.mixin(require("./common"));

var got_error = false;

var promise = posix.readdir(fixturesDir);
puts("readdir " + fixturesDir);

promise.addCallback(function (files) {
  p(files);
  assert.deepEqual(["a.js", "b","cycles", "multipart.js",
    "nested-index","test_ca.pem",
    "test_cert.pem", "test_key.pem", "throws_error.js", "x.txt"], files.sort());
});

promise.addErrback(function () {
  puts("error");
  got_error = true;
});

process.addListener("exit", function () {
  assert.equal(false, got_error);
  puts("exit");
});
