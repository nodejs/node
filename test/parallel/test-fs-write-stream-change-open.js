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

const file = path.join(common.tmpDir, 'write.txt');

common.refreshTmpDir();

const stream = fs.WriteStream(file);
const close = process.binding('fs').close;
const open = process.binding('fs').open;

// change the binding.open with an identical function after the WriteStream
// has pushed it onto its internal action queue, but before it's returned.
process.binding('fs').open = function() {
  return open.apply(null, arguments);
};

process.binding('fs').close = function(fd) {
  assert.ok(fd, 'fs.close must not be called with an undefined fd.');
  process.binding('fs').close = close;
  process.binding('fs').open = open;
};

stream.write('foo');
stream.end();

process.on('exit', function() {
  assert.strictEqual(process.binding('fs').open, open);
});
