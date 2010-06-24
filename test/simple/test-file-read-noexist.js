require("../common");
var path = require('path');
var fs = require('fs');
var got_error = false;

var filename = path.join(fixturesDir, "does_not_exist.txt");
fs.readFile(filename, "raw", function (err, content) {
  if (err) {
    got_error = true;
  } else {
    debug("cat returned some content: " + content);
    debug("this shouldn't happen as the file doesn't exist...");
    assert.equal(true, false);
  }
});

process.addListener("exit", function () {
  console.log("done");
  assert.equal(true, got_error);
});
