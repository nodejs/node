process.mixin(require("./common"));

var fn = path.join(fixturesDir, "write.txt");
var expected = "hello";
var found;

fs.open(fn, process.O_WRONLY | process.O_TRUNC | process.O_CREAT, 0644).addCallback(function (file) {
  fs.write(file, expected, 0, "utf8").addCallback(function() {
    fs.close(file).addCallback(function() {
      fs.cat(fn, process.UTF8).addCallback(function(contents) {
        found = contents;
        fs.unlink(fn).wait();
      });
    });
  });
});

process.addListener("exit", function () {
  assert.equal(expected, found);
});

