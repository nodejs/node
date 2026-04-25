'use strict';

// This tests that Buffer.prototype.copy correctly handles copies whose
// source/target offsets or byte counts exceed 2**32. Refs:
// https://github.com/nodejs/node/issues/55422
const common = require('../common');

// Cannot test on 32-bit machines because the test relies on creating
// buffers larger than 4 GiB.
common.skipIf32Bits();

// Allocating 4+ GiB buffers in CI environments is expensive and may also
// fail under sanitizer / memory-constrained builds. Gate the test behind
// an explicit opt-in env var to avoid timeouts/OOMs in normal CI runs.
if (!process.env.NODE_TEST_LARGE_BUFFER) {
  common.skip('Skipping: requires NODE_TEST_LARGE_BUFFER=1 (allocates >4GiB)');
}

const assert = require('assert');

const threshold = 0xFFFFFFFF; // 2**32 - 1
const overflow = threshold + 5; // 2**32 + 4 — exercises offsets > 2**32

let largeBuffer;
try {
  // Allocate a buffer slightly larger than 2**32 so we can copy to and from
  // offsets above the uint32 boundary.
  largeBuffer = Buffer.alloc(overflow);
} catch (e) {
  if (e.code === 'ERR_MEMORY_ALLOCATION_FAILED' ||
      /Array buffer allocation failed/.test(e.message)) {
    common.skip('insufficient memory for >4GiB Buffer.alloc');
  }
  throw e;
}

// Sentinel byte at an index above 2**32. Before the fix, copy() truncates the
// length to 32 bits and the sentinel never gets written, so reading it back
// would yield 0.
const sentinelIndex = threshold + 2;
largeBuffer[sentinelIndex] = 0xAB;

// Test 1: Buffer.prototype.copy with sourceEnd > 2**32 should copy the bytes
// past the 4 GiB boundary instead of silently truncating.
{
  const target = Buffer.alloc(overflow);
  const copied = largeBuffer.copy(target, 0, 0, overflow);
  assert.strictEqual(copied, overflow,
                     'copy() should report copying the full byte range');
  assert.strictEqual(target[sentinelIndex], 0xAB,
                     'byte beyond 2**32 must be copied, not truncated');
}

// Test 2: Buffer.prototype.copy with targetStart > 2**32 should write at the
// large offset rather than wrapping back to a low address.
{
  const target = Buffer.alloc(overflow);
  const src = Buffer.from([0xCD]);
  const copied = src.copy(target, threshold + 1, 0, 1);
  assert.strictEqual(copied, 1);
  assert.strictEqual(target[threshold + 1], 0xCD,
                     'targetStart > 2**32 must not wrap to a low offset');
  // The low offset that uint32 truncation would have written to must remain
  // untouched.
  assert.strictEqual(target[(threshold + 1) >>> 0], 0);
}

// Test 3: Buffer.concat with a single >4 GiB buffer should preserve the
// trailing bytes, exercising the original repro from the issue.
{
  largeBuffer.fill(0x6F); // 'o'
  const result = Buffer.concat([largeBuffer]);
  assert.strictEqual(result.length, overflow);
  assert.strictEqual(result[overflow - 1], 0x6F,
                     'final byte beyond 2**32 must survive concat');
}
