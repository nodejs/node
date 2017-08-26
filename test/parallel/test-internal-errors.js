// Flags: --expose-internals
'use strict';

const common = require('../common');
const errors = require('internal/errors');
const assert = require('assert');

const errMessages = {
  objectString: /^'object' === 'string'$/,
  booleanString: /^'boolean' === 'string'$/,
  numberString: /^'number' === 'string'$/,
  invalidKey: /^An invalid error message key was used: TEST_FOO_KEY\.$/,
};

errors.E('TEST_ERROR_1', 'Error for testing purposes: %s');
errors.E('TEST_ERROR_2', (a, b) => `${a} ${b}`);

const err1 = new errors.Error('TEST_ERROR_1', 'test');
const err2 = new errors.TypeError('TEST_ERROR_1', 'test');
const err3 = new errors.RangeError('TEST_ERROR_1', 'test');
const err4 = new errors.Error('TEST_ERROR_2', 'abc', 'xyz');
const err5 = new errors.Error('TEST_ERROR_1');

assert(err1 instanceof Error);
assert.strictEqual(err1.name, 'Error [TEST_ERROR_1]');
assert.strictEqual(err1.message, 'Error for testing purposes: test');
assert.strictEqual(err1.code, 'TEST_ERROR_1');

assert(err2 instanceof TypeError);
assert.strictEqual(err2.name, 'TypeError [TEST_ERROR_1]');
assert.strictEqual(err2.message, 'Error for testing purposes: test');
assert.strictEqual(err2.code, 'TEST_ERROR_1');

assert(err3 instanceof RangeError);
assert.strictEqual(err3.name, 'RangeError [TEST_ERROR_1]');
assert.strictEqual(err3.message, 'Error for testing purposes: test');
assert.strictEqual(err3.code, 'TEST_ERROR_1');

assert(err4 instanceof Error);
assert.strictEqual(err4.name, 'Error [TEST_ERROR_2]');
assert.strictEqual(err4.message, 'abc xyz');
assert.strictEqual(err4.code, 'TEST_ERROR_2');

assert(err5 instanceof Error);
assert.strictEqual(err5.name, 'Error [TEST_ERROR_1]');
assert.strictEqual(err5.message, 'Error for testing purposes: %s');
assert.strictEqual(err5.code, 'TEST_ERROR_1');

assert.throws(
  () => new errors.Error('TEST_FOO_KEY'),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.invalidKey
  }));
// Calling it twice yields same result (using the key does not create it)
assert.throws(
  () => new errors.Error('TEST_FOO_KEY'),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.invalidKey
  }));
assert.throws(
  () => new errors.Error(1),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.numberString
  }));
assert.throws(
  () => new errors.Error({}),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.objectString
  }));
assert.throws(
  () => new errors.Error([]),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.objectString
  }));
assert.throws(
  () => new errors.Error(true),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.booleanString
  }));
assert.throws(
  () => new errors.TypeError(1),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.numberString
  }));
assert.throws(
  () => new errors.TypeError({}),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.objectString
  }));
assert.throws(
  () => new errors.TypeError([]),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.objectString
  }));
assert.throws(
  () => new errors.TypeError(true),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.booleanString
  }));
assert.throws(
  () => new errors.RangeError(1),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.numberString
  }));
assert.throws(
  () => new errors.RangeError({}),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.objectString
  }));
assert.throws(
  () => new errors.RangeError([]),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.objectString
  }));
assert.throws(
  () => new errors.RangeError(true),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: errMessages.booleanString
  }));


// Tests for common.expectsError
assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1' }));
});

assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1',
                           type: TypeError,
                           message: /^Error for testing/ }));
});

assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1', type: TypeError }));
});

assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1', type: Error }));
});

assert.throws(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1', type: RangeError }));
}, common.expectsError({
  code: 'ERR_ASSERTION',
  message: /^.+ is not instance of \S/
}));

assert.throws(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1',
                           type: TypeError,
                           message: /^Error for testing 2/ }));
}, common.expectsError({
  code: 'ERR_ASSERTION',
  message: /.+ does not match \S/
}));

// // Test ERR_INVALID_ARG_TYPE
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE', ['a', 'b']),
                   'The "a" argument must be of type b');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE', ['a', ['b']]),
                   'The "a" argument must be of type b');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE', ['a', ['b', 'c']]),
                   'The "a" argument must be one of type b or c');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE',
                                  ['a', ['b', 'c', 'd']]),
                   'The "a" argument must be one of type b, c, or d');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE', ['a', 'b', 'c']),
                   'The "a" argument must be of type b. Received type string');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE',
                                  ['a', 'b', undefined]),
                   'The "a" argument must be of type b. Received type ' +
                   'undefined');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE',
                                  ['a', 'b', null]),
                   'The "a" argument must be of type b. Received type null');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE', ['a', 'not b']),
                   'The "a" argument must not be of type b');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE', ['a.b', 'not c']),
                   'The "a.b" property must not be of type c');
assert.strictEqual(
  errors.message('ERR_INVALID_ARG_TYPE', ['first argument', 'c']),
  'The first argument must be of type c');
assert.strictEqual(
  errors.message('ERR_INVALID_ARG_TYPE', [['a', 'b', 'c'], 'not d']),
  'The "a", "b", "c" arguments must not be of type d');

// Test ERR_INVALID_URL_SCHEME
assert.strictEqual(errors.message('ERR_INVALID_URL_SCHEME', ['file']),
                   'The URL must be of scheme file');
assert.strictEqual(errors.message('ERR_INVALID_URL_SCHEME', [['file']]),
                   'The URL must be of scheme file');
assert.strictEqual(errors.message('ERR_INVALID_URL_SCHEME', [['http', 'ftp']]),
                   'The URL must be one of scheme http or ftp');
assert.strictEqual(errors.message('ERR_INVALID_URL_SCHEME', [['a', 'b', 'c']]),
                   'The URL must be one of scheme a, b, or c');
assert.throws(
  () => errors.message('ERR_INVALID_URL_SCHEME', [[]]),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: /^At least one expected value needs to be specified$/
  }));

// Test ERR_MISSING_ARGS
assert.strictEqual(errors.message('ERR_MISSING_ARGS', ['name']),
                   'The "name" argument must be specified');
assert.strictEqual(errors.message('ERR_MISSING_ARGS', ['name', 'value']),
                   'The "name" and "value" arguments must be specified');
assert.strictEqual(errors.message('ERR_MISSING_ARGS', ['a', 'b', 'c']),
                   'The "a", "b", and "c" arguments must be specified');
assert.throws(
  () => errors.message('ERR_MISSING_ARGS'),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: /^At least one arg needs to be specified$/
  }));


// Test ERR_TLS_CERT_ALTNAME_INVALID
assert.strictEqual(
  errors.message('ERR_TLS_CERT_ALTNAME_INVALID', ['altname']),
  'Hostname/IP does not match certificate\'s altnames: altname');

assert.strictEqual(
  errors.message('ERR_INVALID_PROTOCOL', ['bad protocol', 'http']),
  'Protocol "bad protocol" not supported. Expected "http"'
);

assert.strictEqual(
  errors.message('ERR_HTTP_HEADERS_SENT', ['render']),
  'Cannot render headers after they are sent to the client'
);

assert.strictEqual(
  errors.message('ERR_INVALID_DOMAIN_NAME'),
  'Unable to determine the domain name'
);

assert.strictEqual(
  errors.message('ERR_INVALID_HTTP_TOKEN', ['Method', 'foo']),
  'Method must be a valid HTTP token ["foo"]'
);

assert.strictEqual(
  errors.message('ERR_VALUE_OUT_OF_RANGE', ['A', 'some values', 'B']),
  'The value of "A" must be some values. Received "B"'
);

assert.strictEqual(
  errors.message('ERR_UNESCAPED_CHARACTERS', ['Request path']),
  'Request path contains unescaped characters'
);


// Test error messages for async_hooks
assert.strictEqual(
  errors.message('ERR_ASYNC_CALLBACK', ['init']),
  'init must be a function');
assert.strictEqual(
  errors.message('ERR_ASYNC_TYPE', [{}]),
  'Invalid name for async "type": [object Object]');
assert.strictEqual(
  errors.message('ERR_INVALID_ASYNC_ID', ['asyncId', undefined]),
  'Invalid asyncId value: undefined');
