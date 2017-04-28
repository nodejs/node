'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const readdirDir = common.tmpDir;
const files = ['empty', 'files', 'for', 'just', 'testing'];

// Make sure tmp directory is clean
common.refreshTmpDir();

// Create the necessary files
files.forEach(function(currentFile) {
  fs.closeSync(fs.openSync(`${readdirDir}/${currentFile}`, 'w'));
});

// Check the readdir Sync version
assert.deepStrictEqual(files, fs.readdirSync(readdirDir).sort());

// Check the readdir async version
fs.readdir(readdirDir, common.mustCall(function(err, f) {
  assert.ifError(err);
  assert.deepStrictEqual(files, f.sort());
}));

// readdir() on file should throw ENOTDIR
// https://github.com/joyent/node/issues/1869
assert.throws(function() {
  fs.readdirSync(__filename);
}, /Error: ENOTDIR: not a directory/);

fs.readdir(__filename, common.mustCall(function(e) {
  assert.strictEqual(e.code, 'ENOTDIR');
}));
