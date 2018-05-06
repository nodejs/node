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
const fs = require('fs');
const fsPromises = fs.promises;
const path = require('path');
const util = require('util');

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

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

{
  const file1 = path.join(tmpdir.path, 'a.js');

  const setup = () => {
    // Create file, removing it if it existed previously.
    fs.closeSync(fs.openSync(file1, 'w'));
  };

  common.fsTest('chmod', [file1, mode_async.toString(8), (err) => {
    assert.ifError(err);

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
  }], { setup: setup });
}

{
  const file1 = path.join(tmpdir.path, 'b.js');
  const file2 = path.join(tmpdir.path, 'c.js');

  common.fsTest('open', ['w', (err, fd) => {
    assert.ifError(err);

    let fchmod, close;

    if (typeof fd === 'number') {
      fchmod = fs.fchmod.bind(fs, fd);
      close = fs.close.bind(fs, fd);
    } else {
      fchmod = util.callbackify(fsPromises.fchmod.bind(fs, fd));
      close = util.callbackify(fd.close.bind(fd));
      fd = fd.fd;
    }
    fchmod(mode_async.toString(8), (err) => {
      assert.ifError(err);

      if (common.isWindows) {
        assert.ok((fs.fstatSync(fd).mode & 0o777) & mode_async);
      } else {
        assert.strictEqual(mode_async, fs.fstatSync(fd).mode & 0o777);
      }

      common.expectsError(
        () => fs.fchmod(fd, {}),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          type: TypeError,
          message: 'The "mode" argument must be of type number. ' +
                   'Received type object'
        }
      );

      fs.fchmodSync(fd, mode_sync);
      if (common.isWindows) {
        assert.ok((fs.fstatSync(fd).mode & 0o777) & mode_sync);
      } else {
        assert.strictEqual(mode_sync, fs.fstatSync(fd).mode & 0o777);
      }

      close(assert.ifError);
    });
  }], { differentFiles: [file1, file2] });
}

// lchmod
{
  const file = path.join(tmpdir.path, 'd.js');
  const link = path.join(tmpdir.path, 'symbolic-link');

  const setup = () => {
    fs.closeSync(fs.openSync(file, 'w'));
    fs.symlinkSync(file, link);
  };

  if (fs.lchmod) {
    common.fsTest('lchmod', [link, mode_async, (err) => {
      assert.ifError(err);

      assert.strictEqual(mode_async, fs.lstatSync(link).mode & 0o777);

      fs.lchmodSync(link, mode_sync);
      assert.strictEqual(mode_sync, fs.lstatSync(link).mode & 0o777);

      fs.unlinkSync(link);
    }], { setup: setup });
  }
}

['', false, null, undefined, {}, []].forEach((input) => {
  const checkErr = (e) => {
    assert.strictEqual(e.code, 'ERR_INVALID_ARG_TYPE');
    assert(e instanceof TypeError);
    assert(e.message.includes(typeof input));
    return true;
  };
  common.fsTest('fchmod', [input, 0o000, checkErr], { throws: true });
  assert.throws(() => { fs.fchmodSync(input, 0o000); }, checkErr);
});

[false, 1, {}, [], null, undefined].forEach((input) => {
  const checkErr = (e) => {
    assert.strictEqual(e.code, 'ERR_INVALID_ARG_TYPE');
    assert(e instanceof TypeError);
    assert(e.message.includes(typeof input));
    return true;
  };
  common.fsTest('chmod', [input, 1, checkErr], { throws: true });
  assert.throws(() => fs.chmodSync(input, 1), checkErr);
});

process.on('exit', function() {
  assert.strictEqual(0, openCount);
});
