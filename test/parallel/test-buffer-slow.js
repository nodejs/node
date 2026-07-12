'use strict';

require('../common');
const assert = require('assert');
const { Buffer, kMaxLength } = require('buffer');

const ones = [1, 1, 1, 1];

// Should create a Buffer
let sb = Buffer.allocUnsafeSlow(4);
assert(sb instanceof Buffer);
assert.strictEqual(sb.length, 4);
sb.fill(1);
for (const [key, value] of sb.entries()) {
  assert.deepStrictEqual(value, ones[key]);
}

// underlying ArrayBuffer should have the same length
assert.strictEqual(sb.buffer.byteLength, 4);

// Should work without new
sb = Buffer.allocUnsafeSlow(4);
assert(sb instanceof Buffer);
assert.strictEqual(sb.length, 4);
sb.fill(1);
for (const [key, value] of sb.entries()) {
  assert.deepStrictEqual(value, ones[key]);
}

// Should work with edge cases
assert.strictEqual(Buffer.allocUnsafeSlow(0).length, 0);

// Should throw with invalid length type
const bufferInvalidTypeMsg = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: /^The "size" argument must be of type number/,
};
assert.throws(() => Buffer.allocUnsafeSlow(), bufferInvalidTypeMsg);
assert.throws(() => Buffer.allocUnsafeSlow({}), bufferInvalidTypeMsg);
assert.throws(() => Buffer.allocUnsafeSlow('6'), bufferInvalidTypeMsg);
assert.throws(() => Buffer.allocUnsafeSlow(true), bufferInvalidTypeMsg);

// Should throw with invalid length value
const bufferMaxSizeMsg = {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
};
assert.throws(() => Buffer.allocUnsafeSlow(NaN), bufferMaxSizeMsg);
assert.throws(() => Buffer.allocUnsafeSlow(Infinity), bufferMaxSizeMsg);
assert.throws(() => Buffer.allocUnsafeSlow(-1), bufferMaxSizeMsg);
assert.throws(() => Buffer.allocUnsafeSlow(kMaxLength + 1), bufferMaxSizeMsg);
