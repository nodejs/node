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

let mode_async;
let mode_sync;

// Need to hijack fs.open/close to make sure that things
// get closed once they're opened.
fs._open = fs.open;
fs._openSync = fs.openSync;
fs.open = open;
fs.openSync = openSync;
fs._close = fs.close;
fs._closeSync = fs.closeSync;
fs.close = close;
fs.closeSync = closeSync;

let openCount = 0;

function open() {
  openCount++;
  return fs._open.apply(fs, arguments);
}

function openSync() {
  openCount++;
  return fs._openSync.apply(fs, arguments);
}

function close() {
  openCount--;
  return fs._close.apply(fs, arguments);
}

function closeSync() {
  openCount--;
  return fs._closeSync.apply(fs, arguments);
}


// On Windows chmod is only able to manipulate read-only bit
if (common.isWindows) {
  mode_async = 0o400;   // read-only
  mode_sync = 0o600;    // read-write
} else {
  mode_async = 0o777;
  mode_sync = 0o644;
}

const file1 = path.join(common.fixturesDir, 'a.js');
const file2 = path.join(common.fixturesDir, 'a1.js');

fs.chmod(file1, mode_async.toString(8), common.mustCall((err) => {
  assert.ifError(err);

  console.log(fs.statSync(file1).mode);

  if (common.isWindows) {
    assert.ok((fs.statSync(file1).mode & 0o777) & mode_async);
  } else {
    assert.strictEqual(mode_async, fs.statSync(file1).mode & 0o777);
  }

  fs.chmodSync(file1, mode_sync);
  if (common.isWindows) {
    assert.ok((fs.statSync(file1).mode & 0o777) & mode_sync);
  } else {
    assert.strictEqual(mode_sync, fs.statSync(file1).mode & 0o777);
  }
}));

fs.open(file2, 'a', common.mustCall((err, fd) => {
  assert.ifError(err);

  fs.fchmod(fd, mode_async.toString(8), common.mustCall((err) => {
    assert.ifError(err);

    console.log(fs.fstatSync(fd).mode);

    if (common.isWindows) {
      assert.ok((fs.fstatSync(fd).mode & 0o777) & mode_async);
    } else {
      assert.strictEqual(mode_async, fs.fstatSync(fd).mode & 0o777);
    }

    fs.fchmodSync(fd, mode_sync);
    if (common.isWindows) {
      assert.ok((fs.fstatSync(fd).mode & 0o777) & mode_sync);
    } else {
      assert.strictEqual(mode_sync, fs.fstatSync(fd).mode & 0o777);
    }

    fs.close(fd);
  }));
}));

// lchmod
if (fs.lchmod) {
  const link = path.join(common.tmpDir, 'symbolic-link');

  common.refreshTmpDir();
  fs.symlinkSync(file2, link);

  fs.lchmod(link, mode_async, common.mustCall((err) => {
    assert.ifError(err);

    console.log(fs.lstatSync(link).mode);
    assert.strictEqual(mode_async, fs.lstatSync(link).mode & 0o777);

    fs.lchmodSync(link, mode_sync);
    assert.strictEqual(mode_sync, fs.lstatSync(link).mode & 0o777);

  }));
}


process.on('exit', function() {
  assert.strictEqual(0, openCount);
});
