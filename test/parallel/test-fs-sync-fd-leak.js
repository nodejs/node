// Flags: --expose-internals
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
const { internalBinding } = require('internal/test/binding');

// Ensure that (read|write|append)FileSync() closes the file descriptor
fs.openSync = function() {
  return 42;
};
fs.closeSync = common.mustCall((fd) => {
  assert.strictEqual(fd, 42);
}, 2);
fs.readSync = function() {
  throw new Error('BAM');
};
fs.writeSync = function() {
  throw new Error('BAM');
};

// Internal fast paths are pure C++, can't error inside write
internalBinding('fs').writeFileUtf8 = common.mustCall(function() {
  // Fake close
  throw new Error('BAM');
}, 2);

internalBinding('fs').fstat = function() {
  throw new Error('EBADF: bad file descriptor, fstat');
};

assert.throws(function() {
  // Fast path: writeFileSync utf8
  fs.writeFileSync('dummy', 'xxx');
}, { message: 'BAM' });
assert.throws(function() {
  // Non-fast path
  fs.writeFileSync('dummy', 'xxx', { encoding: 'base64' });
}, { message: 'BAM' });
assert.throws(function() {
  // Fast path: writeFileSync utf8
  fs.appendFileSync('dummy', 'xxx');
}, { message: 'BAM' });
assert.throws(function() {
  // Non-fast path
  fs.appendFileSync('dummy', 'xxx', { encoding: 'base64' });
}, { message: 'BAM' });
