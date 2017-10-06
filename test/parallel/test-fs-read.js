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
const filepath = path.join(common.fixturesDir, 'x.txt');
const fd = fs.openSync(filepath, 'r');

const expected = Buffer.from('xyz\n');

function test(bufferAsync, bufferSync, expected) {
  fs.read(fd,
          bufferAsync,
          0,
          expected.length,
          0,
          common.mustCall((err, bytesRead) => {
            assert.ifError(err);
            assert.strictEqual(bytesRead, expected.length);
            assert.deepStrictEqual(bufferAsync, expected);
          }));

  const r = fs.readSync(fd, bufferSync, 0, expected.length, 0);
  assert.deepStrictEqual(bufferSync, expected);
  assert.strictEqual(r, expected.length);
}

test(Buffer.allocUnsafe(expected.length),
     Buffer.allocUnsafe(expected.length),
     expected);

test(new Uint8Array(expected.length),
     new Uint8Array(expected.length),
     Uint8Array.from(expected));
