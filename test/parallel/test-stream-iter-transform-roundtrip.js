// Flags: --experimental-stream-iter
'use strict';

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
  compressDeflate,
  compressBrotli,
  compressZstd,
  decompressGzip,
  decompressDeflate,
  decompressBrotli,
  decompressZstd,
} = require('zlib/iter');

// =============================================================================
// Helper: compress then decompress, verify round-trip equality
// =============================================================================

async function roundTrip(input, compress, decompress) {
  return text(pull(pull(from(input), compress), decompress));
}

async function roundTripBytes(inputBuf, compress, decompress) {
  return bytes(pull(pull(from(inputBuf), compress), decompress));
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
  const compressed = await bytes(pull(from(inputBuf), compressBrotli()));
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
  const compressed = await bytes(pull(from(inputBuf), compressZstd()));
  assert.ok(compressed.byteLength < inputBuf.byteLength,
            `Compressed ${compressed.byteLength} should be < original ${inputBuf.byteLength}`);
}

// =============================================================================
// Binary data round-trip - verify no corruption on non-text data
// =============================================================================

// Create a buffer with a repeating byte pattern covering all 256 values.
function makeBinaryTestData(size = 1024) {
  const buf = Buffer.alloc(size);
  for (let i = 0; i < size; i++) buf[i] = i & 0xFF;
  return buf;
}

async function testBinaryRoundTripGzip() {
  const input = makeBinaryTestData();
  const result = await roundTripBytes(input, compressGzip(), decompressGzip());
  assert.strictEqual(result.byteLength, input.byteLength);
  assert.deepStrictEqual(Buffer.from(result), input);
}

async function testBinaryRoundTripDeflate() {
  const input = makeBinaryTestData();
  const result = await roundTripBytes(input, compressDeflate(),
                                      decompressDeflate());
  assert.strictEqual(result.byteLength, input.byteLength);
  assert.deepStrictEqual(Buffer.from(result), input);
}

async function testBinaryRoundTripBrotli() {
  const input = makeBinaryTestData();
  const result = await roundTripBytes(input, compressBrotli(),
                                      decompressBrotli());
  assert.strictEqual(result.byteLength, input.byteLength);
  assert.deepStrictEqual(Buffer.from(result), input);
}

async function testBinaryRoundTripZstd() {
  const input = makeBinaryTestData();
  const result = await roundTripBytes(input, compressZstd(), decompressZstd());
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
  // Compress: gzip then deflate
  const compressed = pull(pull(from(input), compressGzip()), compressDeflate());
  // Decompress: deflate then gzip (reverse order)
  const decompressed = pull(pull(compressed, decompressDeflate()),
                            decompressGzip());
  const result = await text(decompressed);
  assert.strictEqual(result, input);
}

// =============================================================================
// Transform protocol: verify each factory returns a proper transform object
// =============================================================================

function testTransformProtocol() {
  [
    compressGzip, compressDeflate, compressBrotli, compressZstd,
    decompressGzip, decompressDeflate, decompressBrotli, decompressZstd,
  ].forEach((factory) => {
    const t = factory();
    assert.strictEqual(typeof t.transform, 'function',
                       `${factory.name}() should have a transform function`);
  });
}

// =============================================================================
// Compression with options
// =============================================================================

async function testGzipWithLevel() {
  const data = 'a'.repeat(10000);
  const level1 = await bytes(pull(from(data), compressGzip({ level: 1 })));
  const level9 = await bytes(pull(from(data), compressGzip({ level: 9 })));
  // Higher compression level should produce smaller output
  assert.ok(level9.length <= level1.length);
  // Both should decompress to original
  const dec1 = await text(pull(from(level1), decompressGzip()));
  const dec9 = await text(pull(from(level9), decompressGzip()));
  assert.strictEqual(dec1, data);
  assert.strictEqual(dec9, data);
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
  testTransformProtocol();

  // Compression with options
  await testGzipWithLevel();
})().then(common.mustCall());
