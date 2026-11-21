'use strict';

// Strings must be decoded before they reach the native handle, even when
// decodeStrings: false is passed. This used to abort the process.

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const input = 'héllo wörld';

const pairs = [
  [zlib.createGzip, zlib.gunzipSync],
  [zlib.createDeflate, zlib.inflateSync],
  [zlib.createDeflateRaw, zlib.inflateRawSync],
  [zlib.createBrotliCompress, zlib.brotliDecompressSync],
  [zlib.createZstdCompress, zlib.zstdDecompressSync],
];

for (const [createCompress, decompressSync] of pairs) {
  const compress = createCompress({ decodeStrings: false });
  const chunks = [];
  compress.on('data', (chunk) => chunks.push(chunk));
  compress.on('end', common.mustCall(() => {
    assert.strictEqual(decompressSync(Buffer.concat(chunks)).toString(), input);
  }));
  compress.end(input);
}

zlib.gzip(input, { decodeStrings: false }, common.mustSucceed((compressed) => {
  assert.strictEqual(zlib.gunzipSync(compressed).toString(), input);
}));
