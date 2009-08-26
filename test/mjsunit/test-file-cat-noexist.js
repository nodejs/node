include("mjsunit.js");
var got_error = false;

var dirname = node.path.dirname(__filename);
var fixtures = node.path.join(dirname, "fixtures");
var filename = node.path.join(fixtures, "does_not_exist.txt");
var promise = node.fs.cat(filename, "raw");

promise.addCallback(function (content) {
  node.debug("cat returned some content: " + content);
  node.debug("this shouldn't happen as the file doesn't exist...");
  assertTrue(false);
});

promise.addErrback(function () {
  got_error = true;
});

process.addListener("exit", function () {
  assertTrue(got_error);
});
