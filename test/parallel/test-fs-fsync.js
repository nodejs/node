'use strict';
const common = require('../common');

const path = require('path');
const fs = require('fs');

var file = path.join(common.fixturesDir, 'a.js');

fs.open(file, 'a', 0o777, common.mustCall(function(err, fd) {
  if (err) throw err;

  fs.fdatasyncSync(fd);

  fs.fsyncSync(fd);

  fs.fdatasync(fd, common.mustCall(function(err) {
    if (err) throw err;
    fs.fsync(fd, common.mustCall(function(err) {
      if (err) throw err;
    }));
  }));
}));
