// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { convertToInt, evenRound } = require('internal/webidl');

assert.strictEqual(evenRound(-0.5), 0);
assert.strictEqual(evenRound(0.5), 0);
assert.strictEqual(evenRound(-1.5), -2);
assert.strictEqual(evenRound(1.5), 2);
assert.strictEqual(evenRound(3.4), 3);
assert.strictEqual(evenRound(4.6), 5);
assert.strictEqual(evenRound(5), 5);
assert.strictEqual(evenRound(6), 6);

// https://webidl.spec.whatwg.org/#abstract-opdef-converttoint
assert.strictEqual(convertToInt('x', 0, 64), 0);
assert.strictEqual(convertToInt('x', 1, 64), 1);
assert.strictEqual(convertToInt('x', -0.5, 64), 0);
assert.strictEqual(convertToInt('x', -0.5, 64, { signed: true }), 0);
assert.strictEqual(convertToInt('x', -1.5, 64, { signed: true }), -1);

// EnforceRange
const OutOfRangeValues = [ NaN, Infinity, -Infinity, 2 ** 53, -(2 ** 53) ];
for (const value of OutOfRangeValues) {
  assert.throws(() => convertToInt('x', value, 64, { enforceRange: true }), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

// Out of range: clamp
assert.strictEqual(convertToInt('x', NaN, 64, { clamp: true }), 0);
assert.strictEqual(convertToInt('x', Infinity, 64, { clamp: true }), Number.MAX_SAFE_INTEGER);
assert.strictEqual(convertToInt('x', -Infinity, 64, { clamp: true }), 0);
assert.strictEqual(convertToInt('x', -Infinity, 64, { signed: true, clamp: true }), Number.MIN_SAFE_INTEGER);
assert.strictEqual(convertToInt('x', 0x1_0000_0000, 32, { clamp: true }), 0xFFFF_FFFF);
assert.strictEqual(convertToInt('x', 0xFFFF_FFFF, 32, { clamp: true }), 0xFFFF_FFFF);
assert.strictEqual(convertToInt('x', 0x8000_0000, 32, { clamp: true, signed: true }), 0x7FFF_FFFF);
assert.strictEqual(convertToInt('x', 0xFFFF_FFFF, 32, { clamp: true, signed: true }), 0x7FFF_FFFF);
assert.strictEqual(convertToInt('x', 0.5, 64, { clamp: true }), 0);
assert.strictEqual(convertToInt('x', 1.5, 64, { clamp: true }), 2);
assert.strictEqual(convertToInt('x', -0.5, 64, { clamp: true }), 0);
assert.strictEqual(convertToInt('x', -0.5, 64, { signed: true, clamp: true }), 0);
assert.strictEqual(convertToInt('x', -1.5, 64, { signed: true, clamp: true }), -2);

// Out of range, step 8.
assert.strictEqual(convertToInt('x', NaN, 64), 0);
assert.strictEqual(convertToInt('x', Infinity, 64), 0);
assert.strictEqual(convertToInt('x', -Infinity, 64), 0);
assert.strictEqual(convertToInt('x', 0x1_0000_0000, 32), 0);
assert.strictEqual(convertToInt('x', 0x1_0000_0001, 32), 1);
assert.strictEqual(convertToInt('x', 0xFFFF_FFFF, 32), 0xFFFF_FFFF);

// Out of range, step 11.
assert.strictEqual(convertToInt('x', 0x8000_0000, 32, { signed: true }), -0x8000_0000);
assert.strictEqual(convertToInt('x', 0xFFF_FFFF, 32, { signed: true }), 0xFFF_FFFF);
