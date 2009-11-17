process.mixin(require("./common"));

var posix = require("posix");
var path = require("path");

var f = path.join(fixturesDir, "x.txt");
var f2 = path.join(fixturesDir, "x2.txt");

var changes = 0;
process.watchFile(f, function () {
  puts(f + " change");
  changes++;
});


setTimeout(function () {
  posix.rename(f, f2).wait();
  posix.rename(f2, f).wait();
  process.unwatchFile(f);
}, 10);


process.addListener("exit", function () {
  assertTrue(changes > 0);
});
