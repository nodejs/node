'use strict';
const common = require('../common');
const assert = require('assert');

const path = require('path');
const fs = require('fs');

const file = path.join(common.fixturesDir, 'a.js');

fs.open(file, 'a', 0o777, common.mustCall(function(err, fd) {
  assert.ifError(err);

  fs.fdatasyncSync(fd);

  fs.fsyncSync(fd);

  fs.fdatasync(fd, common.mustCall(function(err) {
    assert.ifError(err);
    fs.fsync(fd, common.mustCall(function(err) {
      assert.ifError(err);
    }));
  }));
}));
