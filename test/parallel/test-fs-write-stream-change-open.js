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
require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

const file = path.join(tmpdir.path, 'write.txt');

tmpdir.refresh();

const stream = fs.WriteStream(file);
const _fs_close = fs.close;
const _fs_open = fs.open;

// Change the fs.open with an identical function after the WriteStream
// has pushed it onto its internal action queue, but before it's
// returned.  This simulates AOP-style extension of the fs lib.
fs.open = function() {
  return _fs_open.apply(fs, arguments);
};

fs.close = function(fd) {
  assert.ok(fd, 'fs.close must not be called with an undefined fd.');
  fs.close = _fs_close;
  fs.open = _fs_open;
};

stream.write('foo');
stream.end();

process.on('exit', function() {
  assert.strictEqual(fs.open, _fs_open);
});
