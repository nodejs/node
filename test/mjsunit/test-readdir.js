
include("mjsunit.js");

var dirname = node.path.dirname(__filename);
var fixtures = node.path.join(dirname, "fixtures");

var got_error = false;

var promise = node.fs.readdir(fixtures);
puts("readdir " + fixtures);

promise.addCallback(function (files) {
  p(files);
  assertArrayEquals(["b","a.js","x.txt"], files);
});

promise.addErrback(function () {
  puts("error");
  got_error = true;
});

process.addListener("exit", function () {
  assertFalse(got_error);
  puts("exit");
});
