require("../common");
var path = require('path');
var testTxt = path.join(fixturesDir, "x.txt");
var fs = require('fs');

setTimeout(function () {
  // put this in a timeout, just so it doesn't get bunched up with the
  // require() calls..
  N = 30;
  for (var i=0; i < N; i++) {
    console.log("start " + i);
    fs.readFile(testTxt, function(err, data) {
      if (err) {
        console.log("error! " + e);
        process.exit(1);
      } else {
        console.log("finish");
      }
    });
  }
}, 100);


