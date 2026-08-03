'use strict';
// Test decompressing a zstd stream that contains multiple concatenated frames

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const abc = 'abc';
const def = 'def';

const abcEncoded = zlib.zstdCompressSync(abc);
const defEncoded = zlib.zstdCompressSync(def);

const data = Buffer.concat([
  abcEncoded,
  defEncoded,
]);

assert.strictEqual(zlib.zstdDecompressSync(data).toString(), (abc + def));

zlib.zstdDecompress(data, common.mustSucceed((result) => {
  assert.strictEqual(result.toString(), (abc + def));
}));

// Test that the next zstd frame can wrap around the input buffer boundary
[0, 1, 2, 3, 4, defEncoded.length].forEach((offset) => {
  const resultBuffers = [];

  const decompress = zlib.createZstdDecompress()
    .on('error', common.mustNotCall())
    .on('data', (data) => resultBuffers.push(data))
    .on('finish', common.mustCall(() => {
      assert.strictEqual(
        Buffer.concat(resultBuffers).toString(),
        'abcdef',
        `result should match original input (offset = ${offset})`
      );
    }));

  // First write: write "abc" + the first bytes of "def"
  decompress.write(Buffer.concat([
    abcEncoded, defEncoded.slice(0, offset),
  ]));

  // Write remaining bytes of "def"
  decompress.end(defEncoded.slice(offset));
});
