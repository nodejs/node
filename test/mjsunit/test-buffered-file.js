include("common.js");

var testTxt = node.path.join(fixturesDir, "test.txt");

var libDir = node.path.join(testDir, "../../lib");
node.libraryPaths.unshift(libDir);
include("/file.js");

var fileUnlinked = false;

var file = new File(testTxt, "w+");
file.write("hello\n");
file.write("world\n");
setTimeout(function () {
  file.write("hello\n");
  file.write("world\n");
  file.close().addCallback(function () {
    node.error("file closed...");
    var out = node.fs.cat(testTxt).wait();
    print("the file contains: ");
    p(out);
    assertEquals("hello\nworld\nhello\nworld\n", out);
    var file2 = new File(testTxt, "r");
    file2.read(5).addCallback(function (data) {
      puts("read(5): " + JSON.stringify(data));
      assertEquals("hello", data);
      node.fs.unlink(testTxt).addCallback(function () {
        fileUnlinked = true;
      });
    });
    file2.close();
  });
}, 10);

process.addListener("exit", function () {
  assertTrue(fileUnlinked);
  puts("done");
});
