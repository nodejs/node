'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var openCount = 0;
var mode;
var content;

// Need to hijack fs.open/close to make sure that things
// get closed once they're opened.
fs._openSync = fs.openSync;
fs.openSync = openSync;
fs._closeSync = fs.closeSync;
fs.closeSync = closeSync;

// Reset the umask for testing
process.umask(0o000);

// On Windows chmod is only able to manipulate read-only bit. Test if creating
// the file in read-only mode works.
if (common.isWindows) {
  mode = 0o444;
} else {
  mode = 0o755;
}

common.refreshTmpDir();

// Test writeFileSync
var file1 = path.join(common.tmpDir, 'testWriteFileSync.txt');

fs.writeFileSync(file1, '123', {mode: mode});

content = fs.readFileSync(file1, {encoding: 'utf8'});
assert.equal('123', content);

assert.equal(mode, fs.statSync(file1).mode & 0o777);

// Test appendFileSync
var file2 = path.join(common.tmpDir, 'testAppendFileSync.txt');

fs.appendFileSync(file2, 'abc', {mode: mode});

content = fs.readFileSync(file2, {encoding: 'utf8'});
assert.equal('abc', content);

assert.equal(mode, fs.statSync(file2).mode & mode);

// Test writeFileSync with file descriptor
var file3 = path.join(common.tmpDir, 'testWriteFileSyncFd.txt');

var fd = fs.openSync(file3, 'w+', mode);
fs.writeFileSync(fd, '123');
fs.closeSync(fd);

content = fs.readFileSync(file3, {encoding: 'utf8'});
assert.equal('123', content);

assert.equal(mode, fs.statSync(file3).mode & 0o777);

// Verify that all opened files were closed.
assert.equal(0, openCount);

function openSync() {
  openCount++;
  return fs._openSync.apply(fs, arguments);
}

function closeSync() {
  openCount--;
  return fs._closeSync.apply(fs, arguments);
}
