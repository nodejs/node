// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const {toLength} = require('internal/util');
const maxValue = Number.MAX_SAFE_INTEGER;

const expectZero = [
  '0', '-0', NaN, {}, [], {'a': 'b'}, [1, 2], '0x', '0o', '0b', false,
  '', ' ', undefined, null, -1, -1.25, -1.1, -1.9, -Infinity
];
expectZero.forEach(function(value) {
  assert.strictEqual(toLength(value), 0);
});

assert.strictEqual(toLength(maxValue - 1), maxValue - 1);
assert.strictEqual(maxValue, maxValue);
assert.strictEqual(toLength(Infinity), maxValue);
assert.strictEqual(toLength(maxValue + 1), maxValue);


[
  '0x100', '0o100', '0b100', 0x100, -0x100, 0o100, -0o100, 0b100, -0b100, true
].forEach(function(value) {
  assert.strictEqual(toLength(value), +value > 0 ? +value : 0);
});

const expectIntegers = new Map([
  [[1], 1], [[-1], 0], [['1'], 1], [['-1'], 0],
  [3.14, 3], [-3.14, 0], ['3.14', 3], ['-3.14', 0],
]);
expectIntegers.forEach(function(expected, value) {
  assert.strictEqual(toLength(value), expected);
});
