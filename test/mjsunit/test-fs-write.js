process.mixin(require("common.js"));

var path = process.path.join(fixturesDir, "write.txt");
var expected = "hello";
var found;

posix.open(path, process.O_WRONLY | process.O_TRUNC | process.O_CREAT, 0644).addCallback(function (file) {
  posix.write(file, expected, 0, "utf8").addCallback(function() {
    posix.close(file).addCallback(function() {
      posix.cat(path, process.UTF8).addCallback(function(contents) {
        found = contents;
        posix.unlink(path).wait();
      });
    });
  });
});

process.addListener("exit", function () {
  assertEquals(expected, found);
});

