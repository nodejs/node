// Flags: --experimental-stream-iter
'use strict';

// Coverage tests for transform.js: abort cleanup, strategy/params options,
// early consumer exit triggering finally block.

const common = require('../common');
const assert = require('assert');
const {
  from,
  pull,
  bytes,
  text,
} = require('stream/iter');
const {
  compressGzip,
  decompressGzip,
  compressDeflate,
  decompressDeflate,
  compressBrotli,
  decompressBrotli,
  compressZstd,
  decompressZstd,
} = require('zlib/iter');
const { constants } = require('zlib');

async function roundTrip(input, compress, decompress) {
  return text(pull(from(input), compress, decompress));
}

// Abort mid-compression triggers finally cleanup
async function testAbortMidCompression() {
  const ac = new AbortController();
  const largeInput = 'x'.repeat(100_000);
  const compressed = pull(from(largeInput), compressGzip(),
                          { signal: ac.signal });
  const iter = compressed[Symbol.asyncIterator]();

  // Read one batch then abort
  const first = await iter.next();
  assert.strictEqual(first.done, false);
  ac.abort();
  await assert.rejects(iter.next(), { name: 'AbortError' });
}

// Early consumer exit (break from for-await) triggers finally
async function testEarlyConsumerExit() {
  const largeInput = 'y'.repeat(100_000);
  const compressed = pull(from(largeInput), compressGzip());

  // eslint-disable-next-line no-unused-vars
  for await (const batch of compressed) {
    break; // Early exit — should trigger finally block cleanup
  }
  // If we get here without hanging or crashing, cleanup worked
}

// Gzip with explicit strategy option
async function testGzipWithStrategy() {
  const input = 'strategy test data '.repeat(100);
  const c = compressGzip({ strategy: constants.Z_DEFAULT_STRATEGY });
  const result = await roundTrip(input, c, decompressGzip());
  assert.strictEqual(result, input);
}

// Deflate with Z_FIXED strategy
async function testDeflateWithFixedStrategy() {
  const input = 'fixed strategy '.repeat(100);
  const c = compressDeflate({ strategy: constants.Z_FIXED });
  const result = await roundTrip(input, c, decompressDeflate());
  assert.strictEqual(result, input);
}

// Brotli with custom quality param
async function testBrotliWithParams() {
  const input = 'brotli params test '.repeat(100);
  const params = { [constants.BROTLI_PARAM_QUALITY]: 5 };
  const result = await roundTrip(input, compressBrotli({ params }),
                                 decompressBrotli());
  assert.strictEqual(result, input);
}

// Zstd with custom compression level param
async function testZstdWithParams() {
  const input = 'zstd params test '.repeat(100);
  const params = { [constants.ZSTD_c_compressionLevel]: 10 };
  const result = await roundTrip(input, compressZstd({ params }),
                                 decompressZstd());
  assert.strictEqual(result, input);
}

// Gzip with custom chunkSize
async function testGzipWithChunkSize() {
  const input = 'chunk size test';
  const c = compressGzip({ chunkSize: 256 });
  const d = decompressGzip({ chunkSize: 256 });
  const result = await roundTrip(input, c, d);
  assert.strictEqual(result, input);
}

// Invalid chunkSize throws when transform is invoked
async function testInvalidChunkSize() {
  const tx = compressGzip({ chunkSize: 8 });
  await assert.rejects(
    async () => await bytes(pull(from('data'), tx)),
    { code: 'ERR_OUT_OF_RANGE' },
  );
}

Promise.all([
  testAbortMidCompression(),
  testEarlyConsumerExit(),
  testGzipWithStrategy(),
  testDeflateWithFixedStrategy(),
  testBrotliWithParams(),
  testZstdWithParams(),
  testGzipWithChunkSize(),
  testInvalidChunkSize(),
]).then(common.mustCall());
