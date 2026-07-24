// Flags: --expose-internals
'use strict';

require('../common');

// Directly exercises defense-in-depth guards in lib/internal/zip/ that the
// public API cannot reach on this platform - they exist for 32-bit limits,
// or for hypothetical callers that skip the pre-validation every current
// call site performs - so their behavior stays pinned down.

const assert = require('node:assert');
const { test } = require('node:test');
const zlib = require('node:zlib');

const { readSafeUint64 } = require('internal/zip/binary');
const { parseZip64Extra } = require('internal/zip/extra-fields');
const { LocalFileHeader } = require('internal/zip/headers');
const { decodeMemberSync } = require('internal/zip/compression');

test('readSafeUint64 rejects a field extending past the buffer', () => {
  // Every current caller validates the containing range first; the guard
  // protects future call sites.
  assert.throws(() => readSafeUint64(Buffer.alloc(7), 0),
                { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /out of bounds/ });
});

test('parseZip64Extra returns nothing when no field is wanted', () => {
  // Reachable only if a caller asks with no overflow sentinel present;
  // CentralFileHeader always wants at least one field when it calls.
  assert.deepStrictEqual(parseZip64Extra(Buffer.alloc(0), {}), {});
});

test('LocalFileHeader.length reports 0 when the fixed part does not fit', () => {
  // The only current caller passes exactly 30 bytes, so the short-buffer
  // branch cannot trigger through it.
  assert.strictEqual(LocalFileHeader.length(Buffer.alloc(10), 0), 0);
});

test('decodeMemberSync bounds output by the declared size without a caller limit', () => {
  // The public wrappers always pass maxSize (defaulted); without one, the
  // declared-size cap alone must stop an overrun.
  const data = Buffer.alloc(4096, 0x61);
  const compressed = zlib.deflateRawSync(data);
  const info = {
    name: 'lie.txt',
    flags: 0,
    method: 8,
    crc32: zlib.crc32(data),
    uncompressedSize: 16, // Lie: it inflates to 4096 bytes
  };
  assert.throws(() => decodeMemberSync(compressed, info),
                { code: 'ERR_ZIP_ENTRY_CORRUPT', message: /beyond its declared size/ });
});
