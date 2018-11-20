'use strict';

const common = require('../../common');
const test_error = require(`./build/${common.buildType}/test_error`);
const assert = require('assert');
const theError = new Error('Some error');
const theTypeError = new TypeError('Some type error');
const theSyntaxError = new SyntaxError('Some syntax error');
const theRangeError = new RangeError('Some type error');
const theReferenceError = new ReferenceError('Some reference error');
const theURIError = new URIError('Some URI error');
const theEvalError = new EvalError('Some eval error');

class MyError extends Error { }
const myError = new MyError('Some MyError');

// Test that native error object is correctly classed
assert.strictEqual(test_error.checkError(theError), true);

// Test that native type error object is correctly classed
assert.strictEqual(test_error.checkError(theTypeError), true);

// Test that native syntax error object is correctly classed
assert.strictEqual(test_error.checkError(theSyntaxError), true);

// Test that native range error object is correctly classed
assert.strictEqual(test_error.checkError(theRangeError), true);

// Test that native reference error object is correctly classed
assert.strictEqual(test_error.checkError(theReferenceError), true);

// Test that native URI error object is correctly classed
assert.strictEqual(test_error.checkError(theURIError), true);

// Test that native eval error object is correctly classed
assert.strictEqual(test_error.checkError(theEvalError), true);

// Test that class derived from native error is correctly classed
assert.strictEqual(test_error.checkError(myError), true);

// Test that non-error object is correctly classed
assert.strictEqual(test_error.checkError({}), false);

// Test that non-error primitive is correctly classed
assert.strictEqual(test_error.checkError('non-object'), false);

assert.throws(() => {
  test_error.throwExistingError();
}, /^Error: existing error$/);

assert.throws(() => {
  test_error.throwError();
}, /^Error: error$/);

assert.throws(() => {
  test_error.throwRangeError();
}, /^RangeError: range error$/);

assert.throws(() => {
  test_error.throwTypeError();
}, /^TypeError: type error$/);

function testThrowArbitrary(value) {
  assert.throws(
    () => test_error.throwArbitrary(value),
    (err) => {
      assert.strictEqual(err, value);
      return true;
    });
}

testThrowArbitrary(42);
testThrowArbitrary({});
testThrowArbitrary([]);
testThrowArbitrary(Symbol('xyzzy'));
testThrowArbitrary(true);
testThrowArbitrary('ball');
testThrowArbitrary(undefined);
testThrowArbitrary(null);
testThrowArbitrary(NaN);

common.expectsError(
  () => test_error.throwErrorCode(),
  {
    code: 'ERR_TEST_CODE',
    message: 'Error [error]'
  });

common.expectsError(
  () => test_error.throwRangeErrorCode(),
  {
    code: 'ERR_TEST_CODE',
    message: 'RangeError [range error]'
  });

common.expectsError(
  () => test_error.throwTypeErrorCode(),
  {
    code: 'ERR_TEST_CODE',
    message: 'TypeError [type error]'
  });

let error = test_error.createError();
assert.ok(error instanceof Error, 'expected error to be an instance of Error');
assert.strictEqual(error.message, 'error');

error = test_error.createRangeError();
assert.ok(error instanceof RangeError,
          'expected error to be an instance of RangeError');
assert.strictEqual(error.message, 'range error');

error = test_error.createTypeError();
assert.ok(error instanceof TypeError,
          'expected error to be an instance of TypeError');
assert.strictEqual(error.message, 'type error');

error = test_error.createErrorCode();
assert.ok(error instanceof Error, 'expected error to be an instance of Error');
assert.strictEqual(error.code, 'ERR_TEST_CODE');
assert.strictEqual(error.message, 'Error [error]');
assert.strictEqual(error.name, 'Error [ERR_TEST_CODE]');

error = test_error.createRangeErrorCode();
assert.ok(error instanceof RangeError,
          'expected error to be an instance of RangeError');
assert.strictEqual(error.message, 'RangeError [range error]');
assert.strictEqual(error.code, 'ERR_TEST_CODE');
assert.strictEqual(error.name, 'RangeError [ERR_TEST_CODE]');

error = test_error.createTypeErrorCode();
assert.ok(error instanceof TypeError,
          'expected error to be an instance of TypeError');
assert.strictEqual(error.message, 'TypeError [type error]');
assert.strictEqual(error.code, 'ERR_TEST_CODE');
assert.strictEqual(error.name, 'TypeError [ERR_TEST_CODE]');
