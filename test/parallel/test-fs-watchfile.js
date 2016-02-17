'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

// Basic usage tests.
assert.throws(function() {
  fs.watchFile('./some-file');
}, /"watchFile\(\)" requires a listener function/);

assert.throws(function() {
  fs.watchFile('./another-file', {}, 'bad listener');
}, /"watchFile\(\)" requires a listener function/);

assert.throws(function() {
  fs.watchFile(new Object(), function() {});
}, /Path must be a string/);

const enoentFile = path.join(common.tmpDir, 'non-existent-file');
const expectedStatObject = new fs.Stats(
    0,                                        // dev
    0,                                        // mode
    0,                                        // nlink
    0,                                        // uid
    0,                                        // gid
    0,                                        // rdev
    common.isWindows ? undefined : 0,         // blksize
    0,                                        // ino
    0,                                        // size
    common.isWindows ? undefined : 0,         // blocks
    Date.UTC(1970, 0, 1, 0, 0, 0),            // atime
    Date.UTC(1970, 0, 1, 0, 0, 0),            // mtime
    Date.UTC(1970, 0, 1, 0, 0, 0),            // ctime
    Date.UTC(1970, 0, 1, 0, 0, 0)             // birthtime
);

common.refreshTmpDir();

// If the file initially didn't exist, and gets created at a later point of
// time, the callback should be invoked again with proper values in stat object
var fileExists = false;

fs.watchFile(enoentFile, {interval: 0}, common.mustCall(function(curr, prev) {
  if (!fileExists) {
    // If the file does not exist, all the fields should be zero and the date
    // fields should be UNIX EPOCH time
    assert.deepStrictEqual(curr, expectedStatObject);
    assert.deepStrictEqual(prev, expectedStatObject);
    // Create the file now, so that the callback will be called back once the
    // event loop notices it.
    fs.closeSync(fs.openSync(enoentFile, 'w'));
    fileExists = true;
  } else {
    // If the ino (inode) value is greater than zero, it means that the file is
    // present in the filesystem and it has a valid inode number.
    assert(curr.ino > 0);
    // As the file just got created, previous ino value should be lesser than
    // or equal to zero (non-existent file).
    assert(prev.ino <= 0);
    // Stop watching the file
    fs.unwatchFile(enoentFile);
  }
}, 2));
