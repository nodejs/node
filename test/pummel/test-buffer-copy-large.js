'use strict';

// Regression test for https://github.com/nodejs/node/issues/55422
// Buffer.copy and Buffer.concat silently produced incorrect results when
// indices >= 2^32 due to uint32_t overflow in the native SlowCopy path.

const common = require('../common');
const assert = require('assert');

// Cannot test on 32-bit platforms since buffers that large cannot exist.
common.skipIf32Bits();

const THRESHOLD = 2 ** 32; // 4 GiB

// Allocate a large target buffer (just over 4 GiB). Skip if there is not
// enough memory available in the current environment (e.g. CI).
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

// Test 1: Buffer.copy with targetStart >= 2^32
// Copy only the first 5 bytes so _copyActual falls through to the native
// _copy (SlowCopy) instead of using TypedArrayPrototypeSet.
source.copy(target, THRESHOLD, 0, 5);

assert.strictEqual(target[0], 0, 'position 0 must not have been overwritten');
assert.strictEqual(target[THRESHOLD], 111, 'byte at THRESHOLD must be 111');
assert.strictEqual(target[THRESHOLD + 4], 111, 'byte at THRESHOLD+4 must be 111');
assert.strictEqual(target[THRESHOLD + 5], 0, 'byte at THRESHOLD+5 must be 0');

// Test 2: Buffer.copy at the 2^32 - 1 boundary (crossing the 32-bit edge)
target.fill(0);
source.copy(target, THRESHOLD - 2, 0, 5);
assert.strictEqual(target[THRESHOLD - 2], 111, 'byte at boundary start');
assert.strictEqual(target[THRESHOLD + 2], 111, 'byte crossing boundary');
assert.strictEqual(target[THRESHOLD + 3], 0, 'byte after copied range');

// Test 3: Buffer.concat producing a result with total length > 2^32.
// Note: concat uses TypedArrayPrototypeSet (V8), not the native _copy path
// fixed above. This test verifies the V8 path also handles large offsets.
const small = Buffer.alloc(2, 115);
const result = Buffer.concat([target, small]);
assert.strictEqual(result.length, THRESHOLD + 12);
assert.strictEqual(result[THRESHOLD + 10], 115, 'concat: byte from second buffer');
assert.strictEqual(result[THRESHOLD + 11], 115, 'concat: second byte from second buffer');
