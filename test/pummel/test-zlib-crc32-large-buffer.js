'use strict';

// This tests that zlib.crc32() covers the whole input when its length does not
// fit in 32 bits.
const common = require('../common');

common.skipIf32Bits();

const assert = require('assert');
const zlib = require('zlib');

const size = 2 ** 32 + 16;
const split = 2 ** 31;

let buf;
try {
  buf = Buffer.alloc(size, 0x61);
} catch (e) {
  if (e.code === 'ERR_MEMORY_ALLOCATION_FAILED' ||
      /Array buffer allocation failed/.test(e.message)) {
    common.skip('insufficient space for Buffer.alloc');
  }
  throw e;
}

const expected = zlib.crc32(buf.subarray(split),
                            zlib.crc32(buf.subarray(0, split)));
assert.strictEqual(zlib.crc32(buf), expected);
// A length truncated to 32 bits would cover only the first 16 bytes.
assert.notStrictEqual(zlib.crc32(buf), zlib.crc32(buf.subarray(0, 16)));
