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

// On Windows chmod is only able to manipulate read-only bit
if (is_windows) {
  mode_async = 0600;   // read-write
  mode_sync = 0400;    // read-only
} else {
  mode_async = 0777;
  mode_sync = 0644;
}

var file = path.join(common.fixturesDir, 'a.js');

fs.chmod(file, mode_async.toString(8), function(err) {
  if (err) {
    got_error = true;
  } else {
    console.log(fs.statSync(file).mode);

    if (is_windows) {
      assert.ok((fs.statSync(file).mode & 0777) & mode_async);
    } else {
      assert.equal(mode_async, fs.statSync(file).mode & 0777);
    }

    fs.chmodSync(file, mode_sync);
    if (is_windows) {
      assert.ok((fs.statSync(file).mode & 0777) & mode_sync);
    } else {
      assert.equal(mode_sync, fs.statSync(file).mode & 0777);
    }
    success_count++;
  }
});

fs.open(file, 'a', function(err, fd) {
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
    }
  });
});

process.addListener('exit', function() {
  assert.equal(2, success_count);
  assert.equal(false, got_error);
});

