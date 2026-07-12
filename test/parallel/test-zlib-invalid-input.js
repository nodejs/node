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
// Test uncompressing invalid input

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const nonStringInputs = [
  1,
  true,
  { a: 1 },
  ['a'],
];

// zlib.Unzip classes need to get valid data, or else they'll throw.
const unzips = [
  zlib.Unzip(),
  zlib.Gunzip(),
  zlib.Inflate(),
  zlib.InflateRaw(),
  zlib.BrotliDecompress(),
  new zlib.ZstdDecompress(),
];

nonStringInputs.forEach(common.mustCall((input) => {
  assert.throws(() => {
    zlib.gunzip(input);
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE'
  });
}, nonStringInputs.length));

const spoofedLength = new Uint8Array(1).fill(0x41);
Object.defineProperty(spoofedLength, 'length', { get: () => 5000 });
Object.defineProperty(spoofedLength, 'byteLength', { get: () => 5000 });

[
  zlib.deflateSync,
  zlib.gzipSync,
  zlib.deflateRawSync,
  zlib.unzipSync,
  zlib.inflateSync,
  zlib.gunzipSync,
  zlib.inflateRawSync,
  zlib.brotliCompressSync,
  zlib.brotliDecompressSync,
  zlib.zstdCompressSync,
  zlib.zstdDecompressSync,
].forEach((method) => {
  assert.throws(() => {
    method(spoofedLength);
  }, {
    name: 'RangeError',
    code: 'ERR_OUT_OF_RANGE',
  });
});

{
  const deflate = zlib.createDeflate();
  deflate._outOffset = deflate._chunkSize + 1;
  assert.throws(() => {
    deflate._processChunk(Buffer.alloc(1), zlib.constants.Z_FINISH);
  }, {
    name: 'RangeError',
    code: 'ERR_OUT_OF_RANGE',
  });
  deflate.close();
}

unzips.forEach(common.mustCall((uz, i) => {
  uz.on('error', common.mustCall());
  uz.on('end', common.mustNotCall());

  // This will trigger error event
  uz.write('this is not valid compressed data.');
}, unzips.length));
