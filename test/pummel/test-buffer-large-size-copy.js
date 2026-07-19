'use strict';
const common = require('../common');

// Buffer.prototype.copy with offsets and a byte count > 2 ** 32. Regression
// test for the .copy() side of https://github.com/nodejs/node/issues/55422,
// where the byte count and the return value were truncated to 32 bits.
common.skipIf32Bits();

const assert = require('node:assert');

// A little past the 2 ** 32 boundary, with headroom for the shift below.
const size = 2 ** 32 + 16;

let buf;
try {
  buf = Buffer.alloc(size);
} catch (e) {
  if (
    e.code === 'ERR_MEMORY_ALLOCATION_FAILED' ||
    /Array buffer allocation failed/.test(e.message)
  ) {
    common.skip('insufficient memory for Buffer.alloc');
  }

  throw e;
}

// Place a marker near the end of the source range, at an index > 2 ** 32.
const marker = 0x42;
buf[size - 9] = marker;

// Shift the whole buffer right by 8 bytes within itself. `to_copy` is
// size - 8 (> 2 ** 32), so a truncated count would copy only 8 bytes. The
// source byte at size - 9 must land at size - 1.
const copied = buf.copy(buf, 8, 0, size - 8);

// The return value must not be truncated to 32 bits ...
assert.strictEqual(copied, size - 8);
// ... and the byte must actually have moved across the 2 ** 32 boundary.
assert.strictEqual(buf[size - 1], marker);
