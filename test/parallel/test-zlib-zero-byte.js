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

const assert = require('node:assert');
const zlib = require('node:zlib');
const { test } = require('node:test');

test('zlib should properly handle zero byte input', async () => {
  const compressors = [
    [zlib.Gzip, 20],
    [zlib.BrotliCompress, 1],
    [zlib.ZstdCompress, 9],
  ];

  for (const [Compressor, expected] of compressors) {
    const { promise, resolve, reject } = Promise.withResolvers();
    const gz = new Compressor();
    const emptyBuffer = Buffer.alloc(0);
    let received = 0;
    gz.on('data', function(c) {
      received += c.length;
    });
    gz.on('error', reject);
    gz.on('end', function() {
      assert.strictEqual(received, expected,
                         `${received}, ${expected}, ${Compressor.name}`);
      resolve();
    });
    gz.write(emptyBuffer);
    gz.end();
    await promise;
  }
});
