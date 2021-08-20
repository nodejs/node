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
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');

const expected = Buffer.from('xyz\n');

function test(bufferAsync, bufferSync, expected) {
  fs.read(fd,
          bufferAsync,
          0,
          expected.length,
          0,
          common.mustSucceed((bytesRead) => {
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

{
  // Reading beyond file length (3 in this case) should return no data.
  // This is a test for a bug where reads > uint32 would return data
  // from the current position in the file.
  const pos = 0xffffffff + 1; // max-uint32 + 1
  const nRead = fs.readSync(fd, Buffer.alloc(1), 0, 1, pos);
  assert.strictEqual(nRead, 0);

  fs.read(fd, Buffer.alloc(1), 0, 1, pos, common.mustSucceed((nRead) => {
    assert.strictEqual(nRead, 0);
  }));
}

assert.throws(() => new fs.Dir(), {
  code: 'ERR_MISSING_ARGS',
});

assert.throws(
  () => fs.read(fd, Buffer.alloc(1), 0, 1, 0),
  {
    message: 'Callback must be a function. Received undefined',
    code: 'ERR_INVALID_CALLBACK',
  }
);

assert.throws(
  () => fs.read(fd, { buffer: null }, common.mustNotCall()),
  /TypeError: Cannot read property 'byteLength' of null/,
  'throws when options.buffer is null'
);

assert.throws(
  () => fs.readSync(fd, { buffer: null }),
  {
    name: 'TypeError',
    message: 'The "buffer" argument must be an instance of Buffer, ' +
    'TypedArray, or DataView. Received an instance of Object',
  },
  'throws when options.buffer is null'
);

assert.throws(
  () => fs.read(null, Buffer.alloc(1), 0, 1, 0),
  {
    message: 'The "fd" argument must be of type number. Received null',
    code: 'ERR_INVALID_ARG_TYPE',
  }
);
