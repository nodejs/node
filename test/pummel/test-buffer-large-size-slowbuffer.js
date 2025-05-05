'use strict';
const common = require('../common');

// Buffer with size > INT32_MAX
common.skipIf32Bits();

const assert = require('node:assert');
const { Buffer } = require('node:buffer');

const size = 2 ** 31;

// Test slow Buffer with size larger than integer range
try {
  assert.throws(() => Buffer.allocUnsafeSlow(size).toString('utf8'), {
    code: 'ERR_STRING_TOO_LONG',
  });
} catch (e) {
  if (e.code !== 'ERR_MEMORY_ALLOCATION_FAILED') {
    throw e;
  }
  common.skip('insufficient space for slow Buffer allocation');
}
