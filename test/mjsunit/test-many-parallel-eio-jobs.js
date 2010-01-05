process.mixin(require("./common"));
var testTxt = path.join(fixturesDir, "x.txt");
var posix = require('posix');

setTimeout(function () {
  // put this in a timeout, just so it doesn't get bunched up with the
  // require() calls..
  N = 30;
  for (var i=0; i < N; i++) {
    puts("start " + i);
    posix.cat(testTxt).addCallback(function(data) {
      puts("finish");
    }).addErrback(function (e) {
      puts("error! " + e);
      process.exit(1);
    });
  }
}, 100);


