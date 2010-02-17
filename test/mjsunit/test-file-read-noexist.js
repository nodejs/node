process.mixin(require("./common"));
var got_error = false;

var filename = path.join(fixturesDir, "does_not_exist.txt");
var promise = fs.readFile(filename, "raw");

promise.addCallback(function (content) {
  debug("cat returned some content: " + content);
  debug("this shouldn't happen as the file doesn't exist...");
  assert.equal(true, false);
});

promise.addErrback(function () {
  got_error = true;
});

process.addListener("exit", function () {
  puts("done");
  assert.equal(true, got_error);
});
