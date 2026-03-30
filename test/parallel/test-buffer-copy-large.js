// Flags: --max-old-space-size=6144
'use strict';

// Verify that Buffer.prototype.copy() correctly handles source offsets that
// exceed 2^32-1 without silent integer truncation in the native binding.
// Regression test for https://github.com/nodejs/node/issues/55422

require('../common');
const assert = require('assert');

const FOUR_GB = 2 ** 32;

// Allocate a source buffer just large enough to have a few bytes past the
// 4 GiB mark.  If the system cannot satisfy the allocation, skip the test.
let src;
try {
  src = Buffer.alloc(FOUR_GB + 10, 0xaa);
} catch {
  process.exit(0);
}

// Write distinct sentinel bytes at the very end so we can tell whether the
// copy came from offset FOUR_GB or from offset 0 (which would happen if
// source_start were truncated to uint32 and wrapped to zero).
for (let i = 0; i < 10; i++) {
  src[FOUR_GB + i] = i + 1;
}

// Copy only 10 bytes starting from source offset FOUR_GB (> 2^32-1).
// Without the fix, source_start was truncated to 0, reading 0xaa bytes
// instead of the 1..10 sentinel values written above.
const dst = Buffer.alloc(10, 0x00);
src.copy(dst, 0, FOUR_GB, FOUR_GB + 10);

for (let i = 0; i < 10; i++) {
  assert.strictEqual(dst[i], i + 1,
    `byte ${i} should be ${i + 1} (copy from 4GiB+${i})`);
}
