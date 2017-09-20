'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const d = path.join(common.tmpDir, 'dir');

common.refreshTmpDir();

// Make sure the directory does not exist
assert(!common.fileExists(d));
// Create the directory now
fs.mkdirSync(d);
// Make sure the directory exists
assert(common.fileExists(d));
// Try creating again, it should fail with EEXIST
assert.throws(function() {
  fs.mkdirSync(d);
}, /EEXIST: file already exists, mkdir/);
// Remove the directory now
fs.rmdirSync(d);
// Make sure the directory does not exist
assert(!common.fileExists(d));

// Similarly test the Async version
fs.mkdir(d, 0o666, common.mustCall(function(err) {
  assert.ifError(err);

  fs.mkdir(d, 0o666, common.mustCall(function(err) {
    assert.ok(err, 'got no error');
    assert.ok(/^EEXIST/.test(err.message), 'got no EEXIST message');
    assert.strictEqual(err.code, 'EEXIST', 'got no EEXIST code');
    assert.strictEqual(err.path, d, 'got no proper path for EEXIST');

    fs.rmdir(d, assert.ifError);
  }));
}));
