'use strict';

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const dictionary = Buffer.from(
  `Lorem ipsum dolor sit amet, consectetur adipiscing elit. 
  Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. 
  Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.`
);

const input = Buffer.from(
  `Lorem ipsum dolor sit amet, consectetur adipiscing elit. 
  Lorem ipsum dolor sit amet, consectetur adipiscing elit. 
  Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. 
  Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. 
  Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.`
);

zlib.zstdCompress(input, { dictionary }, common.mustSucceed((compressed) => {
  assert(compressed.length < input.length);
  zlib.zstdDecompress(compressed, { dictionary }, common.mustSucceed((decompressed) => {
    assert.strictEqual(decompressed.toString(), input.toString());
  }));
}));

const baseline = zlib.zstdCompressSync(input, { dictionary }).length;

const arrayBuffer = dictionary.buffer.slice(
  dictionary.byteOffset, dictionary.byteOffset + dictionary.byteLength);
const uint8 = new Uint8Array(arrayBuffer);
const dataView = new DataView(arrayBuffer);

for (const dict of [arrayBuffer, uint8, dataView]) {
  assert.strictEqual(zlib.zstdCompressSync(input, { dictionary: dict }).length,
                     baseline);

  const compressed = zlib.zstdCompressSync(input, { dictionary: dict });
  const decompressed = zlib.zstdDecompressSync(compressed, { dictionary: dict });
  assert.strictEqual(decompressed.toString(), input.toString());
}

for (const dictionary of [null, 'string', 123, true, {}, [1, 2, 3]]) {
  const options = { dictionary };
  const expected = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  };

  assert.throws(() => zlib.createZstdCompress(options), expected);
  assert.throws(() => zlib.createZstdDecompress(options), expected);
  assert.throws(() => zlib.zstdCompressSync(input, options), expected);
  assert.throws(() => zlib.zstdDecompressSync(input, options), expected);
  assert.throws(
    () => zlib.zstdCompress(input, options, common.mustNotCall()),
    expected,
  );
  assert.throws(
    () => zlib.zstdDecompress(input, options, common.mustNotCall()),
    expected,
  );
}
