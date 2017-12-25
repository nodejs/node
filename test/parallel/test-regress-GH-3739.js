'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');

let dir = path.resolve(tmpdir.path);

// Make sure that the tmp directory is clean
tmpdir.refresh();

// Make a long path.
for (let i = 0; i < 50; i++) {
  dir = `${dir}/1234567890`;
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
  assert.ifError(err);
});
