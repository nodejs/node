node.mixin(require("common.js"));

var path = node.path.join(fixturesDir, "write.txt");
var expected = "hello";
var found;

node.fs.open(path, node.O_WRONLY | node.O_TRUNC | node.O_CREAT, 0644).addCallback(function (file) {
  node.fs.write(file, expected, 0, "utf8").addCallback(function() {
    node.fs.close(file).addCallback(function() {
      node.fs.cat(path, node.UTF8).addCallback(function(contents) {
        found = contents;
        node.fs.unlink(path).wait();
      });
    });
  });
});

process.addListener("exit", function () {
  assertEquals(expected, found);
});

