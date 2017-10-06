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
assert.strictEqual(test_error.checkError(theError), true,
                   'Error object correctly classed by napi_is_error');

// Test that native type error object is correctly classed
assert.strictEqual(test_error.checkError(theTypeError), true,
                   'Type error object correctly classed by napi_is_error');

// Test that native syntax error object is correctly classed
assert.strictEqual(test_error.checkError(theSyntaxError), true,
                   'Syntax error object correctly classed by napi_is_error');

// Test that native range error object is correctly classed
assert.strictEqual(test_error.checkError(theRangeError), true,
                   'Range error object correctly classed by napi_is_error');

// Test that native reference error object is correctly classed
assert.strictEqual(test_error.checkError(theReferenceError), true,
                   'Reference error object correctly classed by' +
                   ' napi_is_error');

// Test that native URI error object is correctly classed
assert.strictEqual(test_error.checkError(theURIError), true,
                   'URI error object correctly classed by napi_is_error');

// Test that native eval error object is correctly classed
assert.strictEqual(test_error.checkError(theEvalError), true,
                   'Eval error object correctly classed by napi_is_error');

// Test that class derived from native error is correctly classed
assert.strictEqual(test_error.checkError(myError), true,
                   'Class derived from native error correctly classed by' +
                   ' napi_is_error');

// Test that non-error object is correctly classed
assert.strictEqual(test_error.checkError({}), false,
                   'Non-error object correctly classed by napi_is_error');

// Test that non-error primitive is correctly classed
assert.strictEqual(test_error.checkError('non-object'), false,
                   'Non-error primitive correctly classed by napi_is_error');

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

assert.throws(
  () => test_error.throwErrorCode(),
  common.expectsError({
    code: 'ERR_TEST_CODE',
    message: 'Error [error]'
  })
);

assert.throws(
  () => test_error.throwRangeErrorCode(),
  common.expectsError({
    code: 'ERR_TEST_CODE',
    message: 'RangeError [range error]'
  })
);

assert.throws(
  () => test_error.throwTypeErrorCode(),
  common.expectsError({
    code: 'ERR_TEST_CODE',
    message: 'TypeError [type error]'
  })
);

let error = test_error.createError();
assert.ok(error instanceof Error, 'expected error to be an instance of Error');
assert.strictEqual(error.message, 'error', 'expected message to be "error"');

error = test_error.createRangeError();
assert.ok(error instanceof RangeError,
          'expected error to be an instance of RangeError');
assert.strictEqual(error.message,
                   'range error',
                   'expected message to be "range error"');

error = test_error.createTypeError();
assert.ok(error instanceof TypeError,
          'expected error to be an instance of TypeError');
assert.strictEqual(error.message,
                   'type error',
                   'expected message to be "type error"');

error = test_error.createErrorCode();
assert.ok(error instanceof Error, 'expected error to be an instance of Error');
assert.strictEqual(error.code,
                   'ERR_TEST_CODE',
                   'expected code to be "ERR_TEST_CODE"');
assert.strictEqual(error.message,
                   'Error [error]',
                   'expected message to be "Error [error]"');
assert.strictEqual(error.name,
                   'Error [ERR_TEST_CODE]',
                   'expected name to be "Error [ERR_TEST_CODE]"');

error = test_error.createRangeErrorCode();
assert.ok(error instanceof RangeError,
          'expected error to be an instance of RangeError');
assert.strictEqual(error.message,
                   'RangeError [range error]',
                   'expected message to be "RangeError [range error]"');
assert.strictEqual(error.code,
                   'ERR_TEST_CODE',
                   'expected code to be "ERR_TEST_CODE"');
assert.strictEqual(error.name,
                   'RangeError [ERR_TEST_CODE]',
                   'expected name to be "RangeError[ERR_TEST_CODE]"');

error = test_error.createTypeErrorCode();
assert.ok(error instanceof TypeError,
          'expected error to be an instance of TypeError');
assert.strictEqual(error.message,
                   'TypeError [type error]',
                   'expected message to be "TypeError [type error]"');
assert.strictEqual(error.code,
                   'ERR_TEST_CODE',
                   'expected code to be "ERR_TEST_CODE"');
assert.strictEqual(error.name,
                   'TypeError [ERR_TEST_CODE]',
                   'expected name to be "TypeError[ERR_TEST_CODE]"');
