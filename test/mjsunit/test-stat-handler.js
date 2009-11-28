process.mixin(require("./common"));

var path = require("path");

var f = path.join(fixturesDir, "x.txt");
var f2 = path.join(fixturesDir, "x2.txt");

puts("watching for changes of " + f);

var changes = 0;
process.watchFile(f, function (curr, prev) {
  puts(f + " change");
  changes++;
  assertTrue(curr.mtime != prev.mtime);
  process.unwatchFile(f);
});


var File = require("file").File;

var file = new File(f, 'w+');
file.write('xyz\n');
file.close().wait();


process.addListener("exit", function () {
  assertTrue(changes > 0);
});
