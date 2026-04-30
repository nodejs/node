// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  fromSync,
  pullSync,
  bytesSync,
  textSync,
} = require('stream/iter');
const {
  compressGzipSync,
  compressDeflateSync,
  compressBrotliSync,
  compressZstdSync,
  decompressGzipSync,
  decompressDeflateSync,
  decompressBrotliSync,
  decompressZstdSync,
} = require('zlib/iter');

// =============================================================================
// Helper: sync compress then decompress, verify round-trip equality
// =============================================================================

function roundTrip(input, compress, decompress) {
  return textSync(pullSync(pullSync(fromSync(input), compress), decompress));
}

function roundTripBytes(inputBuf, compress, decompress) {
  return bytesSync(pullSync(pullSync(fromSync(inputBuf), compress), decompress));
}

// =============================================================================
// Gzip sync round-trip tests
// =============================================================================

function testGzipRoundTrip() {
  const input = 'Hello, sync gzip compression!';
  const result = roundTrip(input, compressGzipSync(), decompressGzipSync());
  assert.strictEqual(result, input);
}

function testGzipLargeData() {
  const input = 'gzip sync large data test. '.repeat(5000);
  const result = roundTrip(input, compressGzipSync(), decompressGzipSync());
  assert.strictEqual(result, input);
}

function testGzipActuallyCompresses() {
  const input = 'Repeated data compresses well. '.repeat(1000);
  const inputBuf = Buffer.from(input);
  const compressed = bytesSync(pullSync(fromSync(inputBuf),
                                        compressGzipSync()));
  assert.ok(compressed.byteLength < inputBuf.byteLength,
            `Compressed ${compressed.byteLength} should be < ` +
            `original ${inputBuf.byteLength}`);
}

function testGzipBinaryData() {
  const inputBuf = Buffer.alloc(10000);
  for (let i = 0; i < inputBuf.length; i++) inputBuf[i] = i & 0xff;
  const result = roundTripBytes(inputBuf, compressGzipSync(),
                                decompressGzipSync());
  assert.deepStrictEqual(result, inputBuf);
}

// =============================================================================
// Deflate sync round-trip tests
// =============================================================================

function testDeflateRoundTrip() {
  const input = 'Hello, sync deflate compression!';
  const result = roundTrip(input, compressDeflateSync(),
                           decompressDeflateSync());
  assert.strictEqual(result, input);
}

function testDeflateLargeData() {
  const input = 'deflate sync large data test. '.repeat(5000);
  const result = roundTrip(input, compressDeflateSync(),
                           decompressDeflateSync());
  assert.strictEqual(result, input);
}

// =============================================================================
// Brotli sync round-trip tests
// =============================================================================

function testBrotliRoundTrip() {
  const input = 'Hello, sync brotli compression!';
  const result = roundTrip(input, compressBrotliSync(),
                           decompressBrotliSync());
  assert.strictEqual(result, input);
}

function testBrotliLargeData() {
  const input = 'brotli sync large data test. '.repeat(5000);
  const result = roundTrip(input, compressBrotliSync(),
                           decompressBrotliSync());
  assert.strictEqual(result, input);
}

// =============================================================================
// Zstd sync round-trip tests
// =============================================================================

function testZstdRoundTrip() {
  const input = 'Hello, sync zstd compression!';
  const result = roundTrip(input, compressZstdSync(), decompressZstdSync());
  assert.strictEqual(result, input);
}

function testZstdLargeData() {
  const input = 'zstd sync large data test. '.repeat(5000);
  const result = roundTrip(input, compressZstdSync(), decompressZstdSync());
  assert.strictEqual(result, input);
}

// =============================================================================
// Cross-algorithm: compress async-compatible, decompress sync (and vice versa)
// The sync transforms should produce output compatible with the standard format
// =============================================================================

function testGzipWithOptions() {
  const input = 'options test data '.repeat(100);
  const result = roundTrip(input,
                           compressGzipSync({ level: 1 }),
                           decompressGzipSync());
  assert.strictEqual(result, input);
}

function testBrotliWithOptions() {
  const zlib = require('zlib');
  const input = 'brotli options test data '.repeat(100);
  const result = roundTrip(input,
                           compressBrotliSync({
                             params: {
                               [zlib.constants.BROTLI_PARAM_QUALITY]: 3,
                             },
                           }),
                           decompressBrotliSync());
  assert.strictEqual(result, input);
}

// =============================================================================
// Stateless + stateful sync transform pipeline
// =============================================================================

function testMixedStatelessAndStateful() {
  // Uppercase stateless transform + gzip stateful transform
  const upper = (chunks) => {
    if (chunks === null) return null;
    const out = new Array(chunks.length);
    for (let j = 0; j < chunks.length; j++) {
      const src = chunks[j];
      const buf = Buffer.allocUnsafe(src.length);
      for (let i = 0; i < src.length; i++) {
        const b = src[i];
        buf[i] = (b >= 0x61 && b <= 0x7a) ? b - 0x20 : b;
      }
      out[j] = buf;
    }
    return out;
  };

  const input = 'hello world '.repeat(100);
  const result = textSync(
    pullSync(
      pullSync(fromSync(input), upper, compressGzipSync()),
      decompressGzipSync(),
    ),
  );
  assert.strictEqual(result, input.toUpperCase());
}

// =============================================================================
// Early consumer exit (break from for-of) triggers cleanup
// =============================================================================

function testEarlyExit() {
  const input = 'y'.repeat(100_000);
  const compressed = pullSync(fromSync(input), compressGzipSync());

  // eslint-disable-next-line no-unused-vars
  for (const batch of compressed) {
    break; // Early exit - should trigger finally block cleanup
  }
  // If we get here without crashing, cleanup worked
}

// =============================================================================
// Empty input
// =============================================================================

function testEmptyInput() {
  const result = textSync(
    pullSync(
      pullSync(fromSync(''), compressGzipSync()),
      decompressGzipSync(),
    ),
  );
  assert.strictEqual(result, '');
}

// =============================================================================
// Run all tests
// =============================================================================

testGzipRoundTrip();
testGzipLargeData();
testGzipActuallyCompresses();
testGzipBinaryData();
testDeflateRoundTrip();
testDeflateLargeData();
testBrotliRoundTrip();
testBrotliLargeData();
testZstdRoundTrip();
testZstdLargeData();
testGzipWithOptions();
testBrotliWithOptions();
testMixedStatelessAndStateful();
testEarlyExit();
testEmptyInput();

common.mustCall()();
