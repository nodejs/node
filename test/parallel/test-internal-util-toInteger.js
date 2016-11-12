// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const {toInteger} = require('internal/util');

const expectZero = [
  '0', '-0', NaN, {}, [], {'a': 'b'}, [1, 2], '0x', '0o', '0b', false,
  '', ' ', undefined, null
];
expectZero.forEach(function(value) {
  assert.strictEqual(toInteger(value), 0);
});

assert.strictEqual(toInteger(Infinity), Infinity);
assert.strictEqual(toInteger(-Infinity), -Infinity);

const expectSame = [
  '0x100', '0o100', '0b100', 0x100, -0x100, 0o100, -0o100, 0b100, -0b100, true
];
expectSame.forEach(function(value) {
  assert.strictEqual(toInteger(value), +value, `${value} is not an Integer`);
});

const expectIntegers = new Map([
  [[1], 1], [[-1], -1], [['1'], 1], [['-1'], -1],
  [3.14, 3], [-3.14, -3], ['3.14', 3], ['-3.14', -3],
]);
expectIntegers.forEach(function(expected, value) {
  assert.strictEqual(toInteger(value), expected);
});
