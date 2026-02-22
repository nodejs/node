'use strict';

// Regression test for https://github.com/nodejs/node/issues/55422
// Buffer.copy and Buffer.concat silently produced incorrect results when
// indices involved were >= 2^32 due to 32-bit integer overflow in SlowCopy.

const common = require('../common');
const assert = require('assert');

// This test exercises the native SlowCopy path in node_buffer.cc.
// SlowCopy is invoked by _copyActual when the fast TypedArrayPrototypeSet
// path cannot be used (i.e. when sourceStart !== 0 or nb !== sourceLen).

// Cannot test on 32-bit machines since buffers that large cannot exist there.
common.skipIf32Bits();

const THRESHOLD = 2 ** 32; // 4 GiB

// Allocate a large target buffer (just over 4 GiB) to test that a targetStart
// value >= 2^32 is not silently truncated to its lower 32 bits.
let target;
try {
  target = Buffer.alloc(THRESHOLD + 10, 0);
} catch (e) {
  if (e.code === 'ERR_MEMORY_ALLOCATION_FAILED' ||
      /Array buffer allocation failed/.test(e.message)) {
    common.skip('insufficient memory for large buffer allocation');
  }
  throw e;
}

const source = Buffer.alloc(10, 111);

// Copy only the first 5 bytes of source (nb=5, sourceLen=10 → nb !== sourceLen)
// so _copyActual falls through to the native _copy (SlowCopy) instead of
// using TypedArrayPrototypeSet.  The targetStart is THRESHOLD (2^32), which
// previously overflowed to 0 when cast to uint32_t.
source.copy(target, THRESHOLD, 0, 5);

// The 5 copied bytes must appear at position THRESHOLD, not at position 0.
assert.strictEqual(target[0], 0, 'position 0 must not have been overwritten');
assert.strictEqual(target[THRESHOLD], 111, 'byte at THRESHOLD must be 111');
assert.strictEqual(target[THRESHOLD + 4], 111, 'byte at THRESHOLD+4 must be 111');
assert.strictEqual(target[THRESHOLD + 5], 0, 'byte at THRESHOLD+5 must be 0 (not copied)');
