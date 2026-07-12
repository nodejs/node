'use strict';

// This tests that Blob.prototype.slice() works correctly when the size of the
// Blob is outside the range of 32-bit signed integers.
const common = require('../common');

// Buffer with size > INT32_MAX
common.skipIf32Bits();

const assert = require('assert');

const size = 2 ** 31;

try {
  const buf = Buffer.allocUnsafe(size);
  const blob = new Blob([buf]);
  const slicedBlob = blob.slice(size - 1, size);
  assert.strictEqual(slicedBlob.size, 1);
} catch (e) {
  if (e.code === 'ERR_MEMORY_ALLOCATION_FAILED') {
    common.skip('insufficient space for Buffer.allocUnsafe');
  }
  if (/Array buffer allocation failed/.test(e.message)) {
    common.skip('insufficient space for Blob.prototype.slice()');
  }
  throw e;
}
