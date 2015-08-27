'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

var dir = path.resolve(common.tmpDir);

// Make sure that the tmp directory is clean
common.refreshTmpDir();

// Make a long path.
for (var i = 0; i < 50; i++) {
  dir = dir + '/1234567890';
  try {
    fs.mkdirSync(dir, '0777');
  } catch (e) {
    if (e.code !== 'EEXIST') {
      throw e;
    }
  }
}

// Test if file exists synchronously
assert(common.fileExists(dir), 'Directory is not accessible');

// Test if file exists asynchronously
fs.access(dir, function(err) {
  assert(!err, 'Directory is not accessible');
});
