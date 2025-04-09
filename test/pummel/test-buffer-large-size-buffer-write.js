'use strict';
const common = require('../common');

// Buffer with size > INT32_MAX
common.skipIf32Bits();

const assert = require('node:assert');
const kStringMaxLength = require('buffer').constants.MAX_STRING_LENGTH;

const size = 2 ** 31;

// Test Buffer.write with size larger than integer range
try {
  const buf = Buffer.alloc(size);
  assert.strictEqual(buf.write('a', 2, kStringMaxLength), 1);
  assert.strictEqual(buf.write('a', 2, size), 1);
} catch (e) {
  if (e.code !== 'ERR_MEMORY_ALLOCATION_FAILED') {
    throw e;
  }
  common.skip('insufficient space for Buffer.alloc');
}
