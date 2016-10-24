// Flags: --expose-internals
'use strict';

const common = require('../common');
const errors = require('internal/errors');
const assert = require('assert');

errors.E('TEST_ERROR_1', 'Error for testing purposes: %s');
errors.E('TEST_ERROR_2', (a, b) => `${a} ${b}`);

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

assert.throws(
  () => new errors.Error('TEST_FOO_KEY'),
  /^AssertionError: An invalid error message key was used: TEST_FOO_KEY.$/);
// Calling it twice yields same result (using the key does not create it)
assert.throws(
  () => new errors.Error('TEST_FOO_KEY'),
  /^AssertionError: An invalid error message key was used: TEST_FOO_KEY.$/);
assert.throws(
  () => new errors.Error(1),
  /^AssertionError: 'number' === 'string'$/);
assert.throws(
  () => new errors.Error({}),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.Error([]),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.Error(true),
  /^AssertionError: 'boolean' === 'string'$/);
assert.throws(
  () => new errors.TypeError(1),
  /^AssertionError: 'number' === 'string'$/);
assert.throws(
  () => new errors.TypeError({}),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.TypeError([]),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.TypeError(true),
  /^AssertionError: 'boolean' === 'string'$/);
assert.throws(
  () => new errors.RangeError(1),
  /^AssertionError: 'number' === 'string'$/);
assert.throws(
  () => new errors.RangeError({}),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.RangeError([]),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.RangeError(true),
  /^AssertionError: 'boolean' === 'string'$/);


// Tests for common.expectsError
assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError('TEST_ERROR_1'));
});

assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError('TEST_ERROR_1', TypeError, /^Error for testing/));
});

assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError('TEST_ERROR_1', TypeError));
});

assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError('TEST_ERROR_1', Error));
});

assert.throws(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError('TEST_ERROR_1', RangeError));
}, /^AssertionError: error is not the expected type/);

assert.throws(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError('TEST_ERROR_1', TypeError, /^Error for testing 2/));
}, /^AssertionError: error.message does not match/);
