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

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

try {

  const filepath = tmpdir.resolve('large.txt');
  const fd = fs.openSync(filepath, 'w+');
  const offset = 5 * 1024 * 1024 * 1024; // 5GB
  const message = 'Large File';

  fs.ftruncateSync(fd, offset);
  assert.strictEqual(fs.statSync(filepath).size, offset);
  const writeBuf = Buffer.from(message);
  fs.writeSync(fd, writeBuf, 0, writeBuf.length, offset);
  const readBuf = Buffer.allocUnsafe(writeBuf.length);
  fs.readSync(fd, readBuf, 0, readBuf.length, offset);
  assert.strictEqual(readBuf.toString(), message);
  fs.readSync(fd, readBuf, 0, 1, 0);
  assert.strictEqual(readBuf[0], 0);

  // Verify that floating point positions do not throw.
  fs.writeSync(fd, writeBuf, 0, writeBuf.length, 42.000001);
  fs.close(fd, common.mustCall());
} catch (e) {
  if (e.code !== 'ENOSPC') {
    throw e;
  }
  common.skip('insufficient disk space');
}
