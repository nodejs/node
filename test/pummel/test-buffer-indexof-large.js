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

// Offsets outside the int32 range must not be clamped.
// Clear the ranges searched below, as allocUnsafe() does not zero memory.
const before = 2 ** 31 + 10;
const offset = 2 ** 31 + 50;
buffer.fill(0, 0, 100);
buffer.fill(0, 2 ** 31, match);
buffer[60] = 0x0a;
buffer[before] = 0x0a;

for (const value of [0x0a, Buffer.from([0x0a]), '\n']) {
  assert.strictEqual(buffer.indexOf(value, offset), match);
  assert.strictEqual(buffer.lastIndexOf(value, offset), before);
  assert.strictEqual(buffer.includes(value, match + 1), false);
  assert.strictEqual(buffer.indexOf(value, Infinity), -1);
  // buffer.length is 2 ** 31 + 101, so -offset resolves to index 51.
  assert.strictEqual(buffer.indexOf(value, -offset), 60);
  assert.strictEqual(buffer.lastIndexOf(value, -offset), -1);
}
