'use strict';

require('../common');
const assert = require('assert');
const buffer = require('buffer');
const Buffer = buffer.Buffer;
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
  assert.equal(e.message, 'Array buffer allocation failed');
}

// should work with number-coercible values
assert.strictEqual(SlowBuffer('6').length, 6);
assert.strictEqual(SlowBuffer(true).length, 1);

// should create zero-length buffer if parameter is not a number
assert.strictEqual(SlowBuffer().length, 0);
assert.strictEqual(SlowBuffer(NaN).length, 0);
assert.strictEqual(SlowBuffer({}).length, 0);
assert.strictEqual(SlowBuffer('string').length, 0);

// should throw with invalid length
assert.throws(function() {
  SlowBuffer(Infinity);
}, 'invalid Buffer length');
assert.throws(function() {
  SlowBuffer(-1);
}, 'invalid Buffer length');
assert.throws(function() {
  SlowBuffer(buffer.kMaxLength + 1);
}, 'invalid Buffer length');
