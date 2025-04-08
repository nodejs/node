'use strict';
const common = require('../common');

// Buffer with size > INT32_MAX
common.skipIf32Bits();

// Test Buffer size larger than integer range
const { test } = require('node:test');
const assert = require('assert');
const kStringMaxLength = require('buffer').constants.MAX_STRING_LENGTH;

const size = 2 ** 31;

test('Buffer.write with too long size', () => {
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
});
