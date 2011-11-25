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

var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var got_error = false;
var success_count = 0;
var mode_async;
var mode_sync;
var is_windows = process.platform === 'win32';

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

var openCount = 0;

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
if (is_windows) {
  mode_async = 0400;   // read-only
  mode_sync = 0600;    // read-write
} else {
  mode_async = 0777;
  mode_sync = 0644;
}

var file1 = path.join(common.fixturesDir, 'a.js'),
    file2 = path.join(common.fixturesDir, 'a1.js');

fs.chmod(file1, mode_async.toString(8), function(err) {
  if (err) {
    got_error = true;
  } else {
    console.log(fs.statSync(file1).mode);

    if (is_windows) {
      assert.ok((fs.statSync(file1).mode & 0777) & mode_async);
    } else {
      assert.equal(mode_async, fs.statSync(file1).mode & 0777);
    }

    fs.chmodSync(file1, mode_sync);
    if (is_windows) {
      assert.ok((fs.statSync(file1).mode & 0777) & mode_sync);
    } else {
      assert.equal(mode_sync, fs.statSync(file1).mode & 0777);
    }
    success_count++;
  }
});

fs.open(file2, 'a', function(err, fd) {
  if (err) {
    got_error = true;
    console.error(err.stack);
    return;
  }
  fs.fchmod(fd, mode_async.toString(8), function(err) {
    if (err) {
      got_error = true;
    } else {
      console.log(fs.fstatSync(fd).mode);

      if (is_windows) {
        assert.ok((fs.fstatSync(fd).mode & 0777) & mode_async);
      } else {
        assert.equal(mode_async, fs.fstatSync(fd).mode & 0777);
      }

      fs.fchmodSync(fd, mode_sync);
      if (is_windows) {
        assert.ok((fs.fstatSync(fd).mode & 0777) & mode_sync);
      } else {
        assert.equal(mode_sync, fs.fstatSync(fd).mode & 0777);
      }
      success_count++;
      fs.close(fd);
    }
  });
});

// lchmod
if (fs.lchmod) {
  var link = path.join(common.tmpDir, 'symbolic-link');

  try {
    fs.unlinkSync(link);
  } catch (er) {}
  fs.symlinkSync(file2, link);

  fs.lchmod(link, mode_async, function(err) {
    if (err) {
      got_error = true;
    } else {
      console.log(fs.lstatSync(link).mode);
      assert.equal(mode_async, fs.lstatSync(link).mode & 0777);

      fs.lchmodSync(link, mode_sync);
      assert.equal(mode_sync, fs.lstatSync(link).mode & 0777);
      success_count++;
    }
  });
} else {
  success_count++;
}


process.on('exit', function() {
  assert.equal(3, success_count);
  assert.equal(0, openCount);
  assert.equal(false, got_error);
});

