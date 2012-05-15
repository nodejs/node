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

var constants = require('constants');
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

var caughtException = false;
try {
  // should throw ENOENT, not EBADF
  // see https://github.com/joyent/node/pull/1228
  fs.openSync('/path/to/file/that/does/not/exist', 'r');
}
catch (e) {
  assert.equal(e.code, 'ENOENT');
  caughtException = true;
}
assert.ok(caughtException);

var openFd;
fs.open(__filename, 'r', function(err, fd) {
  if (err) {
    throw err;
  }
  openFd = fd;
});

var openFd2;
fs.open(__filename, 'rs', function(err, fd) {
  if (err) {
    throw err;
  }
  openFd2 = fd;
});

process.on('exit', function() {
  assert.ok(openFd);
  assert.ok(openFd2);
});

