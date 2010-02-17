process.mixin(require("./common"));

var path = require("path");

var f = path.join(fixturesDir, "x.txt");
var f2 = path.join(fixturesDir, "x2.txt");

puts("watching for changes of " + f);

var changes = 0;
process.watchFile(f, function (curr, prev) {
  puts(f + " change");
  changes++;
  assert.equal(true, curr.mtime != prev.mtime);
  process.unwatchFile(f);
});


var fs = require("fs");

var fd = fs.openSync(f, "w+");
fs.writeSync(fd, 'xyz\n');
fs.closeSync(fd);

process.addListener("exit", function () {
  assert.equal(true, changes > 0);
});
