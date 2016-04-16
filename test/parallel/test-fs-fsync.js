'use strict';
var common = require('../common');
var assert = require('assert');

var path = require('path');
var fs = require('fs');
var successes = 0;

var file = path.join(common.fixturesDir, 'a.js');

fs.open(file, 'a', 0o777, function(err, fd) {
  if (err) throw err;

  fs.fdatasyncSync(fd);
  successes++;

  fs.fsyncSync(fd);
  successes++;

  fs.fdatasync(fd, function(err) {
    if (err) throw err;
    successes++;
    fs.fsync(fd, function(err) {
      if (err) throw err;
      successes++;
    });
  });
});

process.on('exit', function() {
  assert.equal(4, successes);
});
