process.mixin(require("./common"));

var testTxt = path.join(fixturesDir, "test.txt");

var libDir = path.join(testDir, "../../lib");
require.paths.unshift(libDir);
process.mixin(require("file"));

var fileUnlinked = false;

var file = new File(testTxt, "w+");
file.write("hello\n");
file.write("world\n");
setTimeout(function () {
  file.write("hello\n");
  file.write("world\n");
  file.close().addCallback(function () {
    error("file closed...");
    var out = posix.cat(testTxt).wait();
    print("the file contains: ");
    p(out);
    assert.equal("hello\nworld\nhello\nworld\n", out);
    var file2 = new File(testTxt, "r");
    file2.read(5).addCallback(function (data) {
      puts("read(5): " + JSON.stringify(data));
      assert.equal("hello", data);
      posix.unlink(testTxt).addCallback(function () {
        fileUnlinked = true;
      });
    });
    file2.close();
  });
}, 10);

process.addListener("exit", function () {
  assert.equal(true, fileUnlinked);
  puts("done");
});
