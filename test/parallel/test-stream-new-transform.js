'use strict';

const common = require('../common');
const assert = require('assert');
const {
  from,
  pull,
  bytes,
  text,
  compressGzip,
  compressDeflate,
  compressBrotli,
  compressZstd,
  decompressGzip,
  decompressDeflate,
  decompressBrotli,
  decompressZstd,
} = require('stream/new');

// =============================================================================
// Helper: compress then decompress, verify round-trip equality
// =============================================================================

async function roundTrip(input, compress, decompress) {
  const source = from(input);
  const compressed = pull(source, compress);
  const decompressed = pull(compressed, decompress);
  return text(decompressed);
}

async function roundTripBytes(inputBuf, compress, decompress) {
  const source = from(inputBuf);
  const compressed = pull(source, compress);
  const decompressed = pull(compressed, decompress);
  return bytes(decompressed);
}

// =============================================================================
// Gzip round-trip tests
// =============================================================================

async function testGzipRoundTrip() {
  const input = 'Hello, gzip compression!';
  const result = await roundTrip(input, compressGzip(), decompressGzip());
  assert.strictEqual(result, input);
}

async function testGzipLargeData() {
  // 100KB of repeated text - exercises multi-chunk path
  const input = 'gzip large data test. '.repeat(5000);
  const result = await roundTrip(input, compressGzip(), decompressGzip());
  assert.strictEqual(result, input);
}

async function testGzipActuallyCompresses() {
  const input = 'Repeated data compresses well. '.repeat(1000);
  const inputBuf = Buffer.from(input);
  const source = from(inputBuf);
  const compressed = await bytes(pull(source, compressGzip()));
  assert.ok(compressed.byteLength < inputBuf.byteLength,
            `Compressed ${compressed.byteLength} should be < original ${inputBuf.byteLength}`);
}

// =============================================================================
// Deflate round-trip tests
// =============================================================================

async function testDeflateRoundTrip() {
  const input = 'Hello, deflate compression!';
  const result = await roundTrip(input, compressDeflate(), decompressDeflate());
  assert.strictEqual(result, input);
}

async function testDeflateLargeData() {
  const input = 'deflate large data test. '.repeat(5000);
  const result = await roundTrip(input, compressDeflate(), decompressDeflate());
  assert.strictEqual(result, input);
}

async function testDeflateActuallyCompresses() {
  const input = 'Repeated data compresses well. '.repeat(1000);
  const inputBuf = Buffer.from(input);
  const source = from(inputBuf);
  const compressed = await bytes(pull(source, compressDeflate()));
  assert.ok(compressed.byteLength < inputBuf.byteLength,
            `Compressed ${compressed.byteLength} should be < original ${inputBuf.byteLength}`);
}

// =============================================================================
// Brotli round-trip tests
// =============================================================================

async function testBrotliRoundTrip() {
  const input = 'Hello, brotli compression!';
  const result = await roundTrip(input, compressBrotli(), decompressBrotli());
  assert.strictEqual(result, input);
}

async function testBrotliLargeData() {
  const input = 'brotli large data test. '.repeat(5000);
  const result = await roundTrip(input, compressBrotli(), decompressBrotli());
  assert.strictEqual(result, input);
}

async function testBrotliActuallyCompresses() {
  const input = 'Repeated data compresses well. '.repeat(1000);
  const inputBuf = Buffer.from(input);
  const source = from(inputBuf);
  const compressed = await bytes(pull(source, compressBrotli()));
  assert.ok(compressed.byteLength < inputBuf.byteLength,
            `Compressed ${compressed.byteLength} should be < original ${inputBuf.byteLength}`);
}

// =============================================================================
// Zstd round-trip tests
// =============================================================================

async function testZstdRoundTrip() {
  const input = 'Hello, zstd compression!';
  const result = await roundTrip(input, compressZstd(), decompressZstd());
  assert.strictEqual(result, input);
}

async function testZstdLargeData() {
  const input = 'zstd large data test. '.repeat(5000);
  const result = await roundTrip(input, compressZstd(), decompressZstd());
  assert.strictEqual(result, input);
}

async function testZstdActuallyCompresses() {
  const input = 'Repeated data compresses well. '.repeat(1000);
  const inputBuf = Buffer.from(input);
  const source = from(inputBuf);
  const compressed = await bytes(pull(source, compressZstd()));
  assert.ok(compressed.byteLength < inputBuf.byteLength,
            `Compressed ${compressed.byteLength} should be < original ${inputBuf.byteLength}`);
}

// =============================================================================
// Binary data round-trip - verify no corruption on non-text data
// =============================================================================

async function testBinaryRoundTripGzip() {
  const input = Buffer.alloc(1024);
  for (let i = 0; i < input.length; i++) input[i] = i & 0xFF;
  const result = await roundTripBytes(input, compressGzip(), decompressGzip());
  assert.strictEqual(result.byteLength, input.byteLength);
  assert.deepStrictEqual(Buffer.from(result), input);
}

async function testBinaryRoundTripDeflate() {
  const input = Buffer.alloc(1024);
  for (let i = 0; i < input.length; i++) input[i] = i & 0xFF;
  const result = await roundTripBytes(input, compressDeflate(),
                                      decompressDeflate());
  assert.strictEqual(result.byteLength, input.byteLength);
  assert.deepStrictEqual(Buffer.from(result), input);
}

async function testBinaryRoundTripBrotli() {
  const input = Buffer.alloc(1024);
  for (let i = 0; i < input.length; i++) input[i] = i & 0xFF;
  const result = await roundTripBytes(input, compressBrotli(),
                                      decompressBrotli());
  assert.strictEqual(result.byteLength, input.byteLength);
  assert.deepStrictEqual(Buffer.from(result), input);
}

async function testBinaryRoundTripZstd() {
  const input = Buffer.alloc(1024);
  for (let i = 0; i < input.length; i++) input[i] = i & 0xFF;
  const result = await roundTripBytes(input, compressZstd(),
                                      decompressZstd());
  assert.strictEqual(result.byteLength, input.byteLength);
  assert.deepStrictEqual(Buffer.from(result), input);
}

// =============================================================================
// Empty input
// =============================================================================

async function testEmptyInputGzip() {
  const result = await roundTrip('', compressGzip(), decompressGzip());
  assert.strictEqual(result, '');
}

async function testEmptyInputDeflate() {
  const result = await roundTrip('', compressDeflate(), decompressDeflate());
  assert.strictEqual(result, '');
}

async function testEmptyInputBrotli() {
  const result = await roundTrip('', compressBrotli(), decompressBrotli());
  assert.strictEqual(result, '');
}

async function testEmptyInputZstd() {
  const result = await roundTrip('', compressZstd(), decompressZstd());
  assert.strictEqual(result, '');
}

// =============================================================================
// Chained transforms - compress with one, then another, decompress in reverse
// =============================================================================

async function testChainedGzipDeflate() {
  const input = 'Double compression test data. '.repeat(100);
  const source = from(input);
  // Compress: gzip then deflate
  const compressed = pull(pull(source, compressGzip()), compressDeflate());
  // Decompress: deflate then gzip (reverse order)
  const decompressed = pull(pull(compressed, decompressDeflate()),
                            decompressGzip());
  const result = await text(decompressed);
  assert.strictEqual(result, input);
}

// =============================================================================
// Transform protocol: verify each factory returns a proper transform object
// =============================================================================

async function testTransformProtocol() {
  const factories = [
    compressGzip, compressDeflate, compressBrotli, compressZstd,
    decompressGzip, decompressDeflate, decompressBrotli, decompressZstd,
  ];

  for (const factory of factories) {
    const t = factory();
    assert.strictEqual(typeof t.transform, 'function',
                       `${factory.name}() should have a transform function`);
  }
}

// =============================================================================
// Cross-compatibility: verify gzip/deflate output is compatible with zlib
// =============================================================================

async function testGzipCompatWithZlib() {
  const zlib = require('zlib');
  const { promisify } = require('util');
  const gunzip = promisify(zlib.gunzip);

  const input = 'Cross-compat test with node:zlib. '.repeat(100);
  const source = from(input);
  const compressed = await bytes(pull(source, compressGzip()));

  // Decompress with standard zlib
  const decompressed = await gunzip(compressed);
  assert.strictEqual(decompressed.toString(), input);
}

async function testDeflateCompatWithZlib() {
  const zlib = require('zlib');
  const { promisify } = require('util');
  const inflate = promisify(zlib.inflate);

  const input = 'Cross-compat deflate test. '.repeat(100);
  const source = from(input);
  const compressed = await bytes(pull(source, compressDeflate()));

  // Decompress with standard zlib
  const decompressed = await inflate(compressed);
  assert.strictEqual(decompressed.toString(), input);
}

async function testBrotliCompatWithZlib() {
  const zlib = require('zlib');
  const { promisify } = require('util');
  const brotliDecompress = promisify(zlib.brotliDecompress);

  const input = 'Cross-compat brotli test. '.repeat(100);
  const source = from(input);
  const compressed = await bytes(pull(source, compressBrotli()));

  const decompressed = await brotliDecompress(compressed);
  assert.strictEqual(decompressed.toString(), input);
}

async function testZstdCompatWithZlib() {
  const zlib = require('zlib');
  const { promisify } = require('util');
  const zstdDecompress = promisify(zlib.zstdDecompress);

  const input = 'Cross-compat zstd test. '.repeat(100);
  const source = from(input);
  const compressed = await bytes(pull(source, compressZstd()));

  const decompressed = await zstdDecompress(compressed);
  assert.strictEqual(decompressed.toString(), input);
}

// =============================================================================
// Reverse compat: compress with zlib, decompress with new streams
// =============================================================================

async function testZlibGzipToNewStreams() {
  const zlib = require('zlib');
  const { promisify } = require('util');
  const gzip = promisify(zlib.gzip);

  const input = 'Reverse compat gzip test. '.repeat(100);
  const compressed = await gzip(input);
  const result = await text(pull(from(compressed), decompressGzip()));
  assert.strictEqual(result, input);
}

async function testZlibDeflateToNewStreams() {
  const zlib = require('zlib');
  const { promisify } = require('util');
  const deflate = promisify(zlib.deflate);

  const input = 'Reverse compat deflate test. '.repeat(100);
  const compressed = await deflate(input);
  const result = await text(pull(from(compressed), decompressDeflate()));
  assert.strictEqual(result, input);
}

async function testZlibBrotliToNewStreams() {
  const zlib = require('zlib');
  const { promisify } = require('util');
  const brotliCompress = promisify(zlib.brotliCompress);

  const input = 'Reverse compat brotli test. '.repeat(100);
  const compressed = await brotliCompress(input);
  const result = await text(pull(from(compressed), decompressBrotli()));
  assert.strictEqual(result, input);
}

async function testZlibZstdToNewStreams() {
  const zlib = require('zlib');
  const { promisify } = require('util');
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
  // Gzip
  await testGzipRoundTrip();
  await testGzipLargeData();
  await testGzipActuallyCompresses();

  // Deflate
  await testDeflateRoundTrip();
  await testDeflateLargeData();
  await testDeflateActuallyCompresses();

  // Brotli
  await testBrotliRoundTrip();
  await testBrotliLargeData();
  await testBrotliActuallyCompresses();

  // Zstd
  await testZstdRoundTrip();
  await testZstdLargeData();
  await testZstdActuallyCompresses();

  // Binary data
  await testBinaryRoundTripGzip();
  await testBinaryRoundTripDeflate();
  await testBinaryRoundTripBrotli();
  await testBinaryRoundTripZstd();

  // Empty input
  await testEmptyInputGzip();
  await testEmptyInputDeflate();
  await testEmptyInputBrotli();
  await testEmptyInputZstd();

  // Chained
  await testChainedGzipDeflate();

  // Protocol
  await testTransformProtocol();

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
