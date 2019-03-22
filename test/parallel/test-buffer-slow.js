'use strict';

require('../common');
const assert = require('assert');
const buffer = require('buffer');
const SlowBuffer = buffer.SlowBuffer;

const ones = [1, 1, 1, 1];

// Should create a Buffer
let sb = SlowBuffer(4);
assert(sb instanceof Buffer);
assert.strictEqual(sb.length, 4);
sb.fill(1);
for (const [key, value] of sb.entries()) {
  assert.deepStrictEqual(value, ones[key]);
}

// underlying ArrayBuffer should have the same length
assert.strictEqual(sb.buffer.byteLength, 4);

// Should work without new
sb = SlowBuffer(4);
assert(sb instanceof Buffer);
assert.strictEqual(sb.length, 4);
sb.fill(1);
for (const [key, value] of sb.entries()) {
  assert.deepStrictEqual(value, ones[key]);
}

// Should work with edge cases
assert.strictEqual(SlowBuffer(0).length, 0);
try {
  assert.strictEqual(
    SlowBuffer(buffer.kMaxLength).length, buffer.kMaxLength);
} catch (e) {
  // Don't match on message as it is from the JavaScript engine. V8 and
  // ChakraCore provide different messages.
  assert.strictEqual(e.name, 'RangeError');
}

// Should throw with invalid length type
const bufferInvalidTypeMsg = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: /^The "size" argument must be of type number/,
};
assert.throws(() => SlowBuffer(), bufferInvalidTypeMsg);
assert.throws(() => SlowBuffer({}), bufferInvalidTypeMsg);
assert.throws(() => SlowBuffer('6'), bufferInvalidTypeMsg);
assert.throws(() => SlowBuffer(true), bufferInvalidTypeMsg);

// Should throw with invalid length value
const bufferMaxSizeMsg = {
  code: 'ERR_INVALID_OPT_VALUE',
  name: 'RangeError',
  message: /^The value "[^"]*" is invalid for option "size"$/
};
assert.throws(() => SlowBuffer(NaN), bufferMaxSizeMsg);
assert.throws(() => SlowBuffer(Infinity), bufferMaxSizeMsg);
assert.throws(() => SlowBuffer(-1), bufferMaxSizeMsg);
assert.throws(() => SlowBuffer(buffer.kMaxLength + 1), bufferMaxSizeMsg);
