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
