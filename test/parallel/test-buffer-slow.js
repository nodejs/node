'use strict';

const common = require('../common');
const assert = require('assert');
const buffer = require('buffer');
const SlowBuffer = buffer.SlowBuffer;

const ones = [1, 1, 1, 1];

// should create a Buffer
let sb = SlowBuffer(4);
assert(sb instanceof Buffer);
assert.strictEqual(sb.length, 4);
sb.fill(1);
for (const [key, value] of sb.entries()) {
  assert.deepStrictEqual(value, ones[key]);
}

// underlying ArrayBuffer should have the same length
assert.strictEqual(sb.buffer.byteLength, 4);

// should work without new
sb = SlowBuffer(4);
assert(sb instanceof Buffer);
assert.strictEqual(sb.length, 4);
sb.fill(1);
for (const [key, value] of sb.entries()) {
  assert.deepStrictEqual(value, ones[key]);
}

// should work with edge cases
assert.strictEqual(SlowBuffer(0).length, 0);
try {
  assert.strictEqual(
    SlowBuffer(buffer.kMaxLength).length, buffer.kMaxLength);
} catch (e) {
  // Don't match on message as it is from the JavaScript engine. V8 and
  // ChakraCore provide different messages.
  assert.strictEqual(e.name, 'RangeError');
}

// should work with number-coercible values
assert.strictEqual(SlowBuffer('6').length, 6);
assert.strictEqual(SlowBuffer(true).length, 1);

// should throw with invalid length
const bufferMaxSizeMsg = common.expectsError({
  code: 'ERR_INVALID_OPT_VALUE',
  type: RangeError,
  message: /^The value "[^"]*" is invalid for option "size"$/
}, 7);

assert.throws(() => SlowBuffer(), bufferMaxSizeMsg);
assert.throws(() => SlowBuffer(NaN), bufferMaxSizeMsg);
assert.throws(() => SlowBuffer({}), bufferMaxSizeMsg);
assert.throws(() => SlowBuffer('string'), bufferMaxSizeMsg);
assert.throws(() => SlowBuffer(Infinity), bufferMaxSizeMsg);
assert.throws(() => SlowBuffer(-1), bufferMaxSizeMsg);
assert.throws(() => SlowBuffer(buffer.kMaxLength + 1), bufferMaxSizeMsg);
