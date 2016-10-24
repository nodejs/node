// Flags: --expose-internals
'use strict';

require('../common');
const errors = require('internal/errors');
const assert = require('assert');

errors[Symbol.for('TEST_ERROR_1')] = 'Error for testing purposes: %s';
errors[Symbol.for('TEST_ERROR_2')] = (a, b) => `${a} ${b}`;

const err1 = new errors.Error('TEST_ERROR_1', 'test');
const err2 = new errors.TypeError('TEST_ERROR_1', 'test');
const err3 = new errors.RangeError('TEST_ERROR_1', 'test');
const err4 = new errors.Error('TEST_ERROR_2', 'abc', 'xyz');

assert(err1 instanceof Error);
assert.strictEqual(err1.name, 'Error[TEST_ERROR_1]');
assert.strictEqual(err1.message, 'Error for testing purposes: test');
assert.strictEqual(err1.code, 'TEST_ERROR_1');

assert(err2 instanceof TypeError);
assert.strictEqual(err2.name, 'TypeError[TEST_ERROR_1]');
assert.strictEqual(err2.message, 'Error for testing purposes: test');
assert.strictEqual(err2.code, 'TEST_ERROR_1');

assert(err3 instanceof RangeError);
assert.strictEqual(err3.name, 'RangeError[TEST_ERROR_1]');
assert.strictEqual(err3.message, 'Error for testing purposes: test');
assert.strictEqual(err3.code, 'TEST_ERROR_1');

assert(err4 instanceof Error);
assert.strictEqual(err4.name, 'Error[TEST_ERROR_2]');
assert.strictEqual(err4.message, 'abc xyz');
assert.strictEqual(err4.code, 'TEST_ERROR_2');

assert.throws(() => new errors.Error('TEST_FOO_KEY'),
              /An invalid error message key was used: TEST_FOO_KEY./);
