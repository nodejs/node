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
// test convenience methods with and without options supplied

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

// Must be a multiple of 4 characters in total to test all ArrayBufferView
// types.
const expectStr = 'blah'.repeat(8);
const expectBuf = Buffer.from(expectStr);

const opts = {
  level: 9,
  chunkSize: 1024,
};

for (const [type, expect] of [
  ['string', expectStr],
  ['Buffer', expectBuf],
  ...common.getArrayBufferViews(expectBuf).map((obj) =>
    [obj[Symbol.toStringTag], obj]
  )
]) {
  for (const method of [
    ['gzip', 'gunzip'],
    ['gzip', 'unzip'],
    ['deflate', 'inflate'],
    ['deflateRaw', 'inflateRaw'],
  ]) {
    zlib[method[0]](expect, opts, common.mustCall((err, result) => {
      zlib[method[1]](result, opts, common.mustCall((err, result) => {
        assert.strictEqual(result.toString(), expectStr,
                           `Should get original string after ${method[0]}/` +
                           `${method[1]} ${type} with options.`);
      }));
    }));

    zlib[method[0]](expect, common.mustCall((err, result) => {
      zlib[method[1]](result, common.mustCall((err, result) => {
        assert.strictEqual(result.toString(), expectStr,
                           `Should get original string after ${method[0]}/` +
                           `${method[1]} ${type} without options.`);
      }));
    }));

    {
      const compressed = zlib[method[0] + 'Sync'](expect, opts);
      const decompressed = zlib[method[1] + 'Sync'](compressed, opts);
      assert.strictEqual(decompressed.toString(), expectStr,
                         `Should get original string after ${method[0]}Sync/` +
                         `${method[1]}Sync ${type} with options.`);
    }


    {
      const compressed = zlib[method[0] + 'Sync'](expect);
      const decompressed = zlib[method[1] + 'Sync'](compressed);
      assert.strictEqual(decompressed.toString(), expectStr,
                         `Should get original string after ${method[0]}Sync/` +
                         `${method[1]}Sync ${type} without options.`);
    }
  }
}
