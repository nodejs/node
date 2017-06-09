// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
let openCount = 0;
let mode;
let content;

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
const file1 = path.join(common.tmpDir, 'testWriteFileSync.txt');

fs.writeFileSync(file1, '123', {mode: mode});

content = fs.readFileSync(file1, {encoding: 'utf8'});
assert.strictEqual(content, '123');

assert.strictEqual(fs.statSync(file1).mode & 0o777, mode);

// Test appendFileSync
const file2 = path.join(common.tmpDir, 'testAppendFileSync.txt');

fs.appendFileSync(file2, 'abc', {mode: mode});

content = fs.readFileSync(file2, {encoding: 'utf8'});
assert.strictEqual(content, 'abc');

assert.strictEqual(fs.statSync(file2).mode & mode, mode);

// Test writeFileSync with file descriptor
const file3 = path.join(common.tmpDir, 'testWriteFileSyncFd.txt');

const fd = fs.openSync(file3, 'w+', mode);
fs.writeFileSync(fd, '123');
fs.closeSync(fd);

content = fs.readFileSync(file3, {encoding: 'utf8'});
assert.strictEqual(content, '123');

assert.strictEqual(fs.statSync(file3).mode & 0o777, mode);

// Verify that all opened files were closed.
assert.strictEqual(openCount, 0);

function openSync() {
  openCount++;
  return fs._openSync.apply(fs, arguments);
}

function closeSync() {
  openCount--;
  return fs._closeSync.apply(fs, arguments);
}
