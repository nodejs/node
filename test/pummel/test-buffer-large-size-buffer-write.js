'use strict';
const common = require('../common');

// Buffer with size > INT32_MAX
common.skipIf32Bits();

const assert = require('node:assert');
const kStringMaxLength = require('buffer').constants.MAX_STRING_LENGTH;

const size = 2 ** 31;

let largeBuffer;

try {
  largeBuffer = Buffer.alloc(size);
} catch (e) {
  if (
    e.code === 'ERR_MEMORY_ALLOCATION_FAILED' ||
    /Array buffer allocation failed/.test(e.message)
  ) {
    common.skip('insufficient space for Buffer.alloc');
  }

  throw e;
}

// Test Buffer.write with size larger than integer range
assert.strictEqual(largeBuffer.write('a', 2, kStringMaxLength), 1);
assert.strictEqual(largeBuffer.write('a', 2, size), 1);
