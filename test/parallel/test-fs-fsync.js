'use strict';
var common = require('../common');
var assert = require('assert');

var path = require('path');
var fs = require('fs');
var successes = 0;

var file = path.join(common.fixturesDir, 'a.js');

common.error('open ' + file);

fs.open(file, 'a', 0o777, function(err, fd) {
  common.error('fd ' + fd);
  if (err) throw err;

  fs.fdatasyncSync(fd);
  common.error('fdatasync SYNC: ok');
  successes++;

  fs.fsyncSync(fd);
  common.error('fsync SYNC: ok');
  successes++;

  fs.fdatasync(fd, function(err) {
    if (err) throw err;
    common.error('fdatasync ASYNC: ok');
    successes++;
    fs.fsync(fd, function(err) {
      if (err) throw err;
      common.error('fsync ASYNC: ok');
      successes++;
    });
  });
});

process.on('exit', function() {
  assert.equal(4, successes);
});
