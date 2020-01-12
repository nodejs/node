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


// On Windows chmod is only able to manipulate write permission
if (common.isWindows) {
  mode_async = 0o400;   // read-only
  mode_sync = 0o600;    // read-write
} else {
  mode_async = 0o777;
  mode_sync = 0o644;
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const file1 = path.join(tmpdir.path, 'a.js');
const file2 = path.join(tmpdir.path, 'a1.js');

// Create file1.
fs.closeSync(fs.openSync(file1, 'w'));

fs.chmod(file1, mode_async.toString(8), common.mustCall((err) => {
  assert.ifError(err);

  if (common.isWindows) {
    assert.ok((fs.statSync(file1).mode & 0o777) & mode_async);
  } else {
    assert.strictEqual(fs.statSync(file1).mode & 0o777, mode_async);
  }

  fs.chmodSync(file1, mode_sync);
  if (common.isWindows) {
    assert.ok((fs.statSync(file1).mode & 0o777) & mode_sync);
  } else {
    assert.strictEqual(fs.statSync(file1).mode & 0o777, mode_sync);
  }
}));

fs.open(file2, 'w', common.mustCall((err, fd) => {
  assert.ifError(err);

  fs.fchmod(fd, mode_async.toString(8), common.mustCall((err) => {
    assert.ifError(err);

    if (common.isWindows) {
      assert.ok((fs.fstatSync(fd).mode & 0o777) & mode_async);
    } else {
      assert.strictEqual(fs.fstatSync(fd).mode & 0o777, mode_async);
    }

    assert.throws(
      () => fs.fchmod(fd, {}),
      {
        code: 'ERR_INVALID_ARG_VALUE',
        name: 'TypeError',
        message: 'The argument \'mode\' must be a 32-bit unsigned integer ' +
                 'or an octal string. Received {}'
      }
    );

    fs.fchmodSync(fd, mode_sync);
    if (common.isWindows) {
      assert.ok((fs.fstatSync(fd).mode & 0o777) & mode_sync);
    } else {
      assert.strictEqual(fs.fstatSync(fd).mode & 0o777, mode_sync);
    }

    fs.close(fd, assert.ifError);
  }));
}));

// lchmod
if (fs.lchmod) {
  const link = path.join(tmpdir.path, 'symbolic-link');

  fs.symlinkSync(file2, link);

  fs.lchmod(link, mode_async, common.mustCall((err) => {
    assert.ifError(err);

    assert.strictEqual(fs.lstatSync(link).mode & 0o777, mode_async);

    fs.lchmodSync(link, mode_sync);
    assert.strictEqual(fs.lstatSync(link).mode & 0o777, mode_sync);

  }));
}

[false, 1, {}, [], null, undefined].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "path" argument must be of type string or an instance ' +
             'of Buffer or URL.' +
             common.invalidArgTypeHelper(input)
  };
  assert.throws(() => fs.chmod(input, 1, common.mustNotCall()), errObj);
  assert.throws(() => fs.chmodSync(input, 1), errObj);
});

process.on('exit', function() {
  assert.strictEqual(openCount, 0);
});
