// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');
const { promisify } = require('util');
const {
  from,
  pull,
  bytes,
  text,
} = require('stream/iter');
const {
  compressGzip,
  compressDeflate,
  compressBrotli,
  compressZstd,
  decompressGzip,
  decompressDeflate,
  decompressBrotli,
  decompressZstd,
} = require('zlib/iter');

// =============================================================================
// Cross-compatibility: verify gzip/deflate output is compatible with zlib
// =============================================================================

async function testGzipCompatWithZlib() {
  const gunzip = promisify(zlib.gunzip);

  const input = 'Cross-compat test with node:zlib. '.repeat(100);
  const compressed = await bytes(pull(from(input), compressGzip()));

  // Decompress with standard zlib
  const decompressed = await gunzip(compressed);
  assert.strictEqual(decompressed.toString(), input);
}

async function testDeflateCompatWithZlib() {
  const inflate = promisify(zlib.inflate);

  const input = 'Cross-compat deflate test. '.repeat(100);
  const compressed = await bytes(pull(from(input), compressDeflate()));

  // Decompress with standard zlib
  const decompressed = await inflate(compressed);
  assert.strictEqual(decompressed.toString(), input);
}

async function testBrotliCompatWithZlib() {
  const brotliDecompress = promisify(zlib.brotliDecompress);

  const input = 'Cross-compat brotli test. '.repeat(100);
  const compressed = await bytes(pull(from(input), compressBrotli()));

  const decompressed = await brotliDecompress(compressed);
  assert.strictEqual(decompressed.toString(), input);
}

async function testZstdCompatWithZlib() {
  const zstdDecompress = promisify(zlib.zstdDecompress);

  const input = 'Cross-compat zstd test. '.repeat(100);
  const compressed = await bytes(pull(from(input), compressZstd()));

  const decompressed = await zstdDecompress(compressed);
  assert.strictEqual(decompressed.toString(), input);
}

// =============================================================================
// Reverse compat: compress with zlib, decompress with new streams
// =============================================================================

async function testZlibGzipToNewStreams() {
  const gzip = promisify(zlib.gzip);

  const input = 'Reverse compat gzip test. '.repeat(100);
  const compressed = await gzip(input);
  const result = await text(pull(from(compressed), decompressGzip()));
  assert.strictEqual(result, input);
}

async function testZlibDeflateToNewStreams() {
  const deflate = promisify(zlib.deflate);

  const input = 'Reverse compat deflate test. '.repeat(100);
  const compressed = await deflate(input);
  const result = await text(pull(from(compressed), decompressDeflate()));
  assert.strictEqual(result, input);
}

async function testZlibBrotliToNewStreams() {
  const brotliCompress = promisify(zlib.brotliCompress);

  const input = 'Reverse compat brotli test. '.repeat(100);
  const compressed = await brotliCompress(input);
  const result = await text(pull(from(compressed), decompressBrotli()));
  assert.strictEqual(result, input);
}

async function testZlibZstdToNewStreams() {
  const zstdCompress = promisify(zlib.zstdCompress);

  const input = 'Reverse compat zstd test. '.repeat(100);
  const compressed = await zstdCompress(input);
  const result = await text(pull(from(compressed), decompressZstd()));
  assert.strictEqual(result, input);
}

// =============================================================================
// Run all tests
// =============================================================================

(async () => {
  // Cross-compat: new streams compress → zlib decompress
  await testGzipCompatWithZlib();
  await testDeflateCompatWithZlib();
  await testBrotliCompatWithZlib();
  await testZstdCompatWithZlib();

  // Reverse compat: zlib compress → new streams decompress
  await testZlibGzipToNewStreams();
  await testZlibDeflateToNewStreams();
  await testZlibBrotliToNewStreams();
  await testZlibZstdToNewStreams();
})().then(common.mustCall());
