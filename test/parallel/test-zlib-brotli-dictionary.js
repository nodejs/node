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

// Test with convenience methods (async).
zlib.brotliCompress(input, { dictionary }, common.mustSucceed((compressed) => {
  assert(compressed.length < input.length,
         'compressed data should be smaller with dictionary');
  zlib.brotliDecompress(compressed, { dictionary }, common.mustSucceed((decompressed) => {
    assert.strictEqual(decompressed.toString(), input.toString());
  }));
}));

// Test with streaming API.
{
  const encoder = zlib.createBrotliCompress({ dictionary });
  const decoder = zlib.createBrotliDecompress({ dictionary });

  const chunks = [];
  decoder.on('data', (chunk) => chunks.push(chunk));
  decoder.on('end', common.mustCall(() => {
    const result = Buffer.concat(chunks);
    assert.strictEqual(result.toString(), input.toString());
  }));

  encoder.pipe(decoder);
  encoder.end(input);
}

// Test that dictionary improves compression ratio.
{
  const withDict = zlib.brotliCompressSync(input, { dictionary });
  const withoutDict = zlib.brotliCompressSync(input);

  // Dictionary-based compression should be at least as good as without.
  assert(withDict.length <= withoutDict.length,
         `Dictionary compression (${withDict.length}) should not be ` +
         `larger than non-dictionary compression (${withoutDict.length})`);

  // Verify decompression with dictionary works.
  const decompressed = zlib.brotliDecompressSync(withDict, { dictionary });
  assert.strictEqual(decompressed.toString(), input.toString());
}

// Test that decompression without matching dictionary fails.
{
  const compressed = zlib.brotliCompressSync(input, { dictionary });
  assert.throws(() => {
    zlib.brotliDecompressSync(compressed);
  }, (err) => {
    // The exact error may vary, but decoding should fail without the
    // matching dictionary.
    return err.code === 'ERR_BROTLI_COMPRESSION_FAILED' ||
           err instanceof Error;
  });
}
