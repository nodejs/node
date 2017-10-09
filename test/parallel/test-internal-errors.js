// Flags: --expose-internals
'use strict';
const common = require('../common');

const assert = require('assert');
const errors = require('internal/errors');

function invalidKey(key) {
  return new RegExp(`^An invalid error message key was used: ${key}\\.$`);
}

errors.E('TEST_ERROR_1', 'Error for testing purposes: %s');
errors.E('TEST_ERROR_2', (a, b) => `${a} ${b}`);

{
  const err = new errors.Error('TEST_ERROR_1', 'test');
  assert(err instanceof Error);
  assert.strictEqual(err.name, 'Error [TEST_ERROR_1]');
  assert.strictEqual(err.message, 'Error for testing purposes: test');
  assert.strictEqual(err.code, 'TEST_ERROR_1');
}

{
  const err = new errors.TypeError('TEST_ERROR_1', 'test');
  assert(err instanceof TypeError);
  assert.strictEqual(err.name, 'TypeError [TEST_ERROR_1]');
  assert.strictEqual(err.message, 'Error for testing purposes: test');
  assert.strictEqual(err.code, 'TEST_ERROR_1');
}

{
  const err = new errors.RangeError('TEST_ERROR_1', 'test');
  assert(err instanceof RangeError);
  assert.strictEqual(err.name, 'RangeError [TEST_ERROR_1]');
  assert.strictEqual(err.message, 'Error for testing purposes: test');
  assert.strictEqual(err.code, 'TEST_ERROR_1');
}

{
  const err = new errors.Error('TEST_ERROR_2', 'abc', 'xyz');
  assert(err instanceof Error);
  assert.strictEqual(err.name, 'Error [TEST_ERROR_2]');
  assert.strictEqual(err.message, 'abc xyz');
  assert.strictEqual(err.code, 'TEST_ERROR_2');
}

{
  const err = new errors.Error('TEST_ERROR_1');
  assert(err instanceof Error);
  assert.strictEqual(err.name, 'Error [TEST_ERROR_1]');
  assert.strictEqual(err.message, 'Error for testing purposes: %s');
  assert.strictEqual(err.code, 'TEST_ERROR_1');
}

assert.throws(
  () => new errors.Error('TEST_FOO_KEY'),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey('TEST_FOO_KEY')
  }));
// Calling it twice yields same result (using the key does not create it)
assert.throws(
  () => new errors.Error('TEST_FOO_KEY'),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey('TEST_FOO_KEY')
  }));
assert.throws(
  () => new errors.Error(1),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey(1)
  }));
assert.throws(
  () => new errors.Error({}),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey('\\[object Object\\]')
  }));
assert.throws(
  () => new errors.Error([]),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey('')
  }));
assert.throws(
  () => new errors.Error(true),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey('true')
  }));
assert.throws(
  () => new errors.TypeError(1),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey(1)
  }));
assert.throws(
  () => new errors.TypeError({}),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey('\\[object Object\\]')
  }));
assert.throws(
  () => new errors.TypeError([]),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey('')
  }));
assert.throws(
  () => new errors.TypeError(true),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey('true')
  }));
assert.throws(
  () => new errors.RangeError(1),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey(1)
  }));
assert.throws(
  () => new errors.RangeError({}),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey('\\[object Object\\]')
  }));
assert.throws(
  () => new errors.RangeError([]),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey('')
  }));
assert.throws(
  () => new errors.RangeError(true),
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: invalidKey('true')
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

// Test ERR_ENCODING_NOT_SUPPORTED
assert.strictEqual(
  errors.message('ERR_ENCODING_NOT_SUPPORTED', ['enc']),
  'The "enc" encoding is not supported');

// Test ERR_HTTP2_HEADER_REQUIRED
assert.strictEqual(
  errors.message('ERR_HTTP2_HEADER_REQUIRED', ['test']),
  'The test header is required');

// Test ERR_HTTP2_FRAME_ERROR
assert.strictEqual(
  errors.message('ERR_HTTP2_FRAME_ERROR', ['foo', 'bar', 'baz']),
  'Error sending frame type foo for stream baz with code bar');
assert.strictEqual(
  errors.message('ERR_HTTP2_FRAME_ERROR', ['foo', 'bar']),
  'Error sending frame type foo with code bar');


// Test that `code` property is mutable and that changing it does not change the
// name.
{
  const myError = new errors.Error('ERR_MISSING_ARGS', ['Sterrance']);
  assert.strictEqual(myError.code, 'ERR_MISSING_ARGS');
  assert.strictEqual(myError.hasOwnProperty('code'), false);
  assert.strictEqual(myError.hasOwnProperty('name'), false);
  assert.deepStrictEqual(Object.keys(myError), []);
  const initialName = myError.name;
  myError.code = 'FHQWHGADS';
  assert.strictEqual(myError.code, 'FHQWHGADS');
  assert.strictEqual(myError.name, initialName);
  assert.deepStrictEqual(Object.keys(myError), ['code']);
  assert.ok(myError.name.includes('ERR_MISSING_ARGS'));
  assert.ok(!myError.name.includes('FHQWHGADS'));
}

// Test that `name` is mutable and that changing it alters `toString()` but not
// `console.log()` results, which is the behavior of `Error` objects in the
// browser. Note that `name` becomes enumerable after being assigned.
{
  const myError = new errors.Error('ERR_MISSING_ARGS', ['Sterrance']);
  assert.deepStrictEqual(Object.keys(myError), []);
  const initialToString = myError.toString();

  myError.name = 'Fhqwhgads';
  assert.deepStrictEqual(Object.keys(myError), ['name']);
  assert.notStrictEqual(myError.toString(), initialToString);
}

// Test that `message` is mutable and that changing it alters `toString()` but
// not `console.log()` results, which is the behavior of `Error` objects in the
// browser. Note that `message` remains non-enumerable after being assigned.
{
  let initialConsoleLog = '';
  common.hijackStdout((data) => { initialConsoleLog += data; });
  const myError = new errors.Error('ERR_MISSING_ARGS', ['Sterrance']);
  assert.deepStrictEqual(Object.keys(myError), []);
  const initialToString = myError.toString();
  console.log(myError);
  assert.notStrictEqual(initialConsoleLog, '');

  common.restoreStdout();

  let subsequentConsoleLog = '';
  common.hijackStdout((data) => { subsequentConsoleLog += data; });
  myError.message = 'Fhqwhgads';
  assert.deepStrictEqual(Object.keys(myError), []);
  assert.notStrictEqual(myError.toString(), initialToString);
  console.log(myError);
  assert.strictEqual(subsequentConsoleLog, initialConsoleLog);

  common.restoreStdout();
}
