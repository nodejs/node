process.mixin(require("./common"));

var fn = path.join(fixturesDir, "write.txt");
var expected = "hello";
var found;

posix.open(fn, process.O_WRONLY | process.O_TRUNC | process.O_CREAT, 0644).addCallback(function (file) {
  posix.write(file, expected, 0, "utf8").addCallback(function() {
    posix.close(file).addCallback(function() {
      posix.cat(fn, process.UTF8).addCallback(function(contents) {
        found = contents;
        posix.unlink(fn).wait();
      });
    });
  });
});

process.addListener("exit", function () {
  assert.equal(expected, found);
});

