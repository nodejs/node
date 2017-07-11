'use strict';

const common = require('../common');
const assert = require('assert');

const rangeBuffer = Buffer.from('abc');

// if start >= buffer's length, empty string will be returned
assert.strictEqual(rangeBuffer.toString('ascii', 3), '');
assert.strictEqual(rangeBuffer.toString('ascii', +Infinity), '');
assert.strictEqual(rangeBuffer.toString('ascii', 3.14, 3), '');
assert.strictEqual(rangeBuffer.toString('ascii', 'Infinity', 3), '');

// if end <= 0, empty string will be returned
assert.strictEqual(rangeBuffer.toString('ascii', 1, 0), '');
assert.strictEqual(rangeBuffer.toString('ascii', 1, -1.2), '');
assert.strictEqual(rangeBuffer.toString('ascii', 1, -100), '');
assert.strictEqual(rangeBuffer.toString('ascii', 1, -Infinity), '');

// if start < 0, start will be taken as zero
assert.strictEqual(rangeBuffer.toString('ascii', -1, 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', -1.99, 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', -Infinity, 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', '-1', 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', '-1.99', 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', '-Infinity', 3), 'abc');

// if start is an invalid integer, start will be taken as zero
assert.strictEqual(rangeBuffer.toString('ascii', 'node.js', 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', {}, 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', [], 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', NaN, 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', null, 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', undefined, 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', false, 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', '', 3), 'abc');

// but, if start is an integer when coerced, then it will be coerced and used.
assert.strictEqual(rangeBuffer.toString('ascii', '-1', 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', '1', 3), 'bc');
assert.strictEqual(rangeBuffer.toString('ascii', '-Infinity', 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', '3', 3), '');
assert.strictEqual(rangeBuffer.toString('ascii', Number(3), 3), '');
assert.strictEqual(rangeBuffer.toString('ascii', '3.14', 3), '');
assert.strictEqual(rangeBuffer.toString('ascii', '1.99', 3), 'bc');
assert.strictEqual(rangeBuffer.toString('ascii', '-1.99', 3), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', 1.99, 3), 'bc');
assert.strictEqual(rangeBuffer.toString('ascii', true, 3), 'bc');

// if end > buffer's length, end will be taken as buffer's length
assert.strictEqual(rangeBuffer.toString('ascii', 0, 5), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', 0, 6.99), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', 0, Infinity), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', 0, '5'), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', 0, '6.99'), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', 0, 'Infinity'), 'abc');

// if end is an invalid integer, end will be taken as buffer's length
assert.strictEqual(rangeBuffer.toString('ascii', 0, 'node.js'), '');
assert.strictEqual(rangeBuffer.toString('ascii', 0, {}), '');
assert.strictEqual(rangeBuffer.toString('ascii', 0, NaN), '');
assert.strictEqual(rangeBuffer.toString('ascii', 0, undefined), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', 0), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', 0, null), '');
assert.strictEqual(rangeBuffer.toString('ascii', 0, []), '');
assert.strictEqual(rangeBuffer.toString('ascii', 0, false), '');
assert.strictEqual(rangeBuffer.toString('ascii', 0, ''), '');

// but, if end is an integer when coerced, then it will be coerced and used.
assert.strictEqual(rangeBuffer.toString('ascii', 0, '-1'), '');
assert.strictEqual(rangeBuffer.toString('ascii', 0, '1'), 'a');
assert.strictEqual(rangeBuffer.toString('ascii', 0, '-Infinity'), '');
assert.strictEqual(rangeBuffer.toString('ascii', 0, '3'), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', 0, Number(3)), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', 0, '3.14'), 'abc');
assert.strictEqual(rangeBuffer.toString('ascii', 0, '1.99'), 'a');
assert.strictEqual(rangeBuffer.toString('ascii', 0, '-1.99'), '');
assert.strictEqual(rangeBuffer.toString('ascii', 0, 1.99), 'a');
assert.strictEqual(rangeBuffer.toString('ascii', 0, true), 'a');

// try toString() with a object as a encoding
assert.strictEqual(rangeBuffer.toString({ toString: function() {
  return 'ascii';
} }), 'abc');

// try toString() with 0 and null as the encoding
assert.throws(() => {
  rangeBuffer.toString(0, 1, 2);
}, common.expectsError({
  code: 'ERR_UNKNOWN_ENCODING',
  type: TypeError,
  message: 'Unknown encoding: 0'
}));
assert.throws(() => {
  rangeBuffer.toString(null, 1, 2);
}, common.expectsError({
  code: 'ERR_UNKNOWN_ENCODING',
  type: TypeError,
  message: 'Unknown encoding: null'
}));
