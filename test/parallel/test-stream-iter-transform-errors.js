// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  from,
  pull,
  bytes,
} = require('stream/iter');
const {
  decompressGzip,
  decompressDeflate,
  decompressBrotli,
  decompressZstd,
} = require('zlib/iter');

// =============================================================================
// Decompression of corrupt data
// =============================================================================

async function testCorruptGzipData() {
  const corrupt = new Uint8Array([0x1F, 0x8B, 0xFF, 0xFF, 0xFF]);

  await assert.rejects(
    async () => await bytes(pull(from(corrupt), decompressGzip())), {
      name: 'Error',
      code: 'Z_DATA_ERROR',
    });
}

async function testCorruptDeflateData() {
  const corrupt = new Uint8Array([0x78, 0xFF, 0xFF, 0xFF]);

  await assert.rejects(
    async () => await bytes(pull(from(corrupt), decompressDeflate())), {
      name: 'Error',
      code: 'Z_DATA_ERROR',
    });
}

async function testCorruptBrotliData() {
  const corrupt = new Uint8Array([0xFF, 0xFF, 0xFF, 0xFF]);

  await assert.rejects(
    async () => await bytes(pull(from(corrupt), decompressBrotli())), {
      name: 'Error',
      code: 'ERR__ERROR_FORMAT_PADDING_2',
    });
}

async function testCorruptZstdData() {
  // Completely invalid data (not even valid magic bytes)
  const corrupt = new Uint8Array([0xFF, 0xFF, 0xFF, 0xFF, 0xFF]);
  await assert.rejects(
    async () => await bytes(pull(from(corrupt), decompressZstd())), {
      name: 'Error',
      code: 'ZSTD_error_prefix_unknown',
    });
}

// =============================================================================
// Run all tests
// =============================================================================

(async () => {
  await testCorruptGzipData();
  await testCorruptDeflateData();
  await testCorruptBrotliData();
  await testCorruptZstdData();
})().then(common.mustCall());
