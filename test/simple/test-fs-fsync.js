require("../common");

var path = require('path');
var fs = require('fs');
var successes = 0;

var file = path.join(fixturesDir, "a.js");

error("open " + file);

fs.open(file, "a", 0777, function (err, fd) {
  error("fd " + fd);
  if (err) throw err;

  fs.fdatasyncSync(fd);
  error("fdatasync SYNC: ok");
  successes++;

  fs.fsyncSync(fd);
  error("fsync SYNC: ok");
  successes++;

  fs.fdatasync(fd, function (err) {
    if (err) throw err;
    error("fdatasync ASYNC: ok");
    successes++;
    fs.fsync(fd, function(err) {
      if (err) throw err;
      error("fsync ASYNC: ok");
      successes++;
    });
  });
});

process.addListener("exit", function () {
  assert.equal(4, successes);
});
