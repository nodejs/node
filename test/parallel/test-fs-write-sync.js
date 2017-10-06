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
const filename = path.join(common.tmpDir, 'write.txt');

common.refreshTmpDir();

// fs.writeSync with all parameters provided:
{
  const fd = fs.openSync(filename, 'w');

  let written = fs.writeSync(fd, '');
  assert.strictEqual(0, written);

  fs.writeSync(fd, 'foo');

  written = fs.writeSync(fd, Buffer.from('bár'), 0, Buffer.byteLength('bár'));
  assert.ok(written > 3);
  fs.closeSync(fd);

  assert.strictEqual(fs.readFileSync(filename, 'utf-8'), 'foobár');
}

// fs.writeSync with a buffer, without the length parameter:
{
  const fd = fs.openSync(filename, 'w');

  let written = fs.writeSync(fd, '');
  assert.strictEqual(0, written);

  fs.writeSync(fd, 'foo');

  written = fs.writeSync(fd, Buffer.from('bár'), 0);
  assert.ok(written > 3);
  fs.closeSync(fd);

  assert.strictEqual(fs.readFileSync(filename, 'utf-8'), 'foobár');
}

// fs.writeSync with a buffer, without the offset and length parameters:
{
  const fd = fs.openSync(filename, 'w');

  let written = fs.writeSync(fd, '');
  assert.strictEqual(0, written);

  fs.writeSync(fd, 'foo');

  written = fs.writeSync(fd, Buffer.from('bár'));
  assert.ok(written > 3);
  fs.closeSync(fd);

  assert.strictEqual(fs.readFileSync(filename, 'utf-8'), 'foobár');
}
