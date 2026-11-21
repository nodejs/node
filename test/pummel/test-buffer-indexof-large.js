'use strict';

const common = require('../common');
const assert = require('assert');

common.skipIf32Bits();

const match = 2 ** 31 + 100;
let buffer;
try {
  buffer = Buffer.allocUnsafe(match + 1);
} catch (error) {
  if (error.code === 'ERR_MEMORY_ALLOCATION_FAILED' ||
      /Array buffer allocation failed/.test(error.message)) {
    common.skip('insufficient space for Buffer.allocUnsafe');
  }
  throw error;
}

buffer[match] = 0x0a;

assert.strictEqual(buffer.indexOf(0x0a, -1), match);
assert.strictEqual(buffer.indexOf(Buffer.from([0x0a]), -1), match);
assert.strictEqual(buffer.indexOf('\n', -1), match);
assert.strictEqual(buffer.lastIndexOf(0x0a, -1), match);
assert.strictEqual(buffer.lastIndexOf(Buffer.from([0x0a]), -1), match);
assert.strictEqual(buffer.lastIndexOf('\n', -1), match);
