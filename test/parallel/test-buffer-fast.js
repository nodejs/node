'use strict';

require('../common');
const assert = require('assert');
const buffer = require('buffer');
const { FastBuffer } = buffer;

const ones = [1, 1, 1, 1];

// Should create a Buffer
const fb = new FastBuffer(4);
assert(fb instanceof Buffer);
assert.strictEqual(fb.length, 4);
fb.fill(1);
for (const [key, value] of fb.entries()) {
  assert.deepStrictEqual(value, ones[key]);
}

// Underlying ArrayBuffer should have the same length
assert.strictEqual(fb.buffer.byteLength, 4);

// Should not work without new
assert.throws(() => FastBuffer(4), /new/);

// Should work with edge cases
assert.strictEqual((new FastBuffer()).length, 0);
assert.strictEqual((new FastBuffer({})).length, 0);
assert.strictEqual((new FastBuffer(0)).length, 0);
assert.strictEqual((new FastBuffer('6')).length, 6);
assert.strictEqual((new FastBuffer(true)).length, 1);
assert.strictEqual((new FastBuffer(NaN)).length, 0);
try {
  assert.strictEqual(
    (new FastBuffer(buffer.kMaxLength)).length, buffer.kMaxLength);
} catch (e) {
  // Don't match on message as it is from the JavaScript engine. V8 and
  // ChakraCore provide different messages.
  assert.strictEqual(e.name, 'RangeError');
}

// Should throw with invalid length value
const bufferMaxSizeMsg = {
  name: 'RangeError',
  message: /^Invalid typed array length/i
};
assert.throws(() => new FastBuffer(Infinity), bufferMaxSizeMsg);
assert.throws(() => new FastBuffer(-1), bufferMaxSizeMsg);
assert.throws(() => new FastBuffer(buffer.kMaxLength + 1), bufferMaxSizeMsg);
