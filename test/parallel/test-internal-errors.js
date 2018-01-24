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

common.expectsError(
  () => new errors.Error('TEST_FOO_KEY'),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey('TEST_FOO_KEY')
  });
// Calling it twice yields same result (using the key does not create it)
common.expectsError(
  () => new errors.Error('TEST_FOO_KEY'),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey('TEST_FOO_KEY')
  });
common.expectsError(
  () => new errors.Error(1),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey(1)
  });
common.expectsError(
  () => new errors.Error({}),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey('\\[object Object\\]')
  });
common.expectsError(
  () => new errors.Error([]),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey('')
  });
common.expectsError(
  () => new errors.Error(true),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey('true')
  });
common.expectsError(
  () => new errors.TypeError(1),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey(1)
  });
common.expectsError(
  () => new errors.TypeError({}),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey('\\[object Object\\]')
  });
common.expectsError(
  () => new errors.TypeError([]),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey('')
  });
common.expectsError(
  () => new errors.TypeError(true),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey('true')
  });
common.expectsError(
  () => new errors.RangeError(1),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey(1)
  });
common.expectsError(
  () => new errors.RangeError({}),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey('\\[object Object\\]')
  });
common.expectsError(
  () => new errors.RangeError([]),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey('')
  });
common.expectsError(
  () => new errors.RangeError(true),
  {
    code: 'ERR_ASSERTION',
    message: invalidKey('true')
  });


// Tests for common.expectsError
assert.doesNotThrow(() => {
  common.expectsError(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, { code: 'TEST_ERROR_1' });
});

assert.doesNotThrow(() => {
  common.expectsError(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, { code: 'TEST_ERROR_1',
       type: TypeError,
       message: /^Error for testing/ });
});

assert.doesNotThrow(() => {
  common.expectsError(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, { code: 'TEST_ERROR_1', type: TypeError });
});

assert.doesNotThrow(() => {
  common.expectsError(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, {
    code: 'TEST_ERROR_1',
    type: TypeError,
    message: 'Error for testing purposes: a'
  });
});

common.expectsError(() => {
  common.expectsError(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, { code: 'TEST_ERROR_1', type: RangeError });
}, {
  code: 'ERR_ASSERTION',
  message: /^.+ is not instance of \S/
});

common.expectsError(() => {
  common.expectsError(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, { code: 'TEST_ERROR_1',
       type: TypeError,
       message: /^Error for testing 2/ });
}, {
  code: 'ERR_ASSERTION',
  type: assert.AssertionError,
  message: /.+ does not match \S/
});

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

// Test ERR_INVALID_FD_TYPE
assert.strictEqual(errors.message('ERR_INVALID_FD_TYPE', ['a']),
                   'Unsupported fd type: a');

// Test ERR_INVALID_URL_SCHEME
assert.strictEqual(errors.message('ERR_INVALID_URL_SCHEME', ['file']),
                   'The URL must be of scheme file');
assert.strictEqual(errors.message('ERR_INVALID_URL_SCHEME', [['file']]),
                   'The URL must be of scheme file');
assert.strictEqual(errors.message('ERR_INVALID_URL_SCHEME', [['http', 'ftp']]),
                   'The URL must be one of scheme http or ftp');
assert.strictEqual(errors.message('ERR_INVALID_URL_SCHEME', [['a', 'b', 'c']]),
                   'The URL must be one of scheme a, b, or c');
common.expectsError(
  () => errors.message('ERR_INVALID_URL_SCHEME', [[]]),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: /^At least one expected value needs to be specified$/
  });

// Test ERR_MISSING_ARGS
assert.strictEqual(errors.message('ERR_MISSING_ARGS', ['name']),
                   'The "name" argument must be specified');
assert.strictEqual(errors.message('ERR_MISSING_ARGS', ['name', 'value']),
                   'The "name" and "value" arguments must be specified');
assert.strictEqual(errors.message('ERR_MISSING_ARGS', ['a', 'b', 'c']),
                   'The "a", "b", and "c" arguments must be specified');
common.expectsError(
  () => errors.message('ERR_MISSING_ARGS'),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: /^At least one arg needs to be specified$/
  });

// Test ERR_SOCKET_BAD_PORT
assert.strictEqual(
  errors.message('ERR_SOCKET_BAD_PORT', [0]),
  'Port should be > 0 and < 65536. Received 0.');

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
  errors.message('ERR_OUT_OF_RANGE', ['A']),
  'The value of "A" is out of range.'
);

assert.strictEqual(
  errors.message('ERR_OUT_OF_RANGE', ['A', 'some values']),
  'The value of "A" is out of range. It must be some values.'
);

assert.strictEqual(
  errors.message('ERR_OUT_OF_RANGE', ['A', 'some values', 'B']),
  'The value of "A" is out of range. It must be some values. Received B'
);

assert.strictEqual(
  errors.message('ERR_UNESCAPED_CHARACTERS', ['Request path']),
  'Request path contains unescaped characters'
);

// Test ERR_DNS_SET_SERVERS_FAILED
assert.strictEqual(
  errors.message('ERR_DNS_SET_SERVERS_FAILED', ['err', 'servers']),
  'c-ares failed to set servers: "err" [servers]');

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

{
  const { kMaxLength } = process.binding('buffer');
  const error = new errors.Error('ERR_BUFFER_TOO_LARGE');
  assert.strictEqual(
    error.message,
    `Cannot create a Buffer larger than 0x${kMaxLength.toString(16)} bytes`
  );
}

{
  const error = new errors.Error('ERR_INVALID_ARG_VALUE', 'foo', '\u0000bar');
  assert.strictEqual(
    error.message,
    'The argument \'foo\' is invalid. Received \'\\u0000bar\''
  );
}

{
  const error = new errors.Error(
    'ERR_INVALID_ARG_VALUE',
    'foo', { a: 1 }, 'must have property \'b\'');
  assert.strictEqual(
    error.message,
    'The argument \'foo\' must have property \'b\'. Received { a: 1 }'
  );
}

// Test that `code` property is mutable and that changing it does not change the
// name.
{
  const myError = new errors.Error('ERR_TLS_HANDSHAKE_TIMEOUT');
  assert.strictEqual(myError.code, 'ERR_TLS_HANDSHAKE_TIMEOUT');
  assert.strictEqual(myError.hasOwnProperty('code'), false);
  assert.strictEqual(myError.hasOwnProperty('name'), false);
  assert.deepStrictEqual(Object.keys(myError), []);
  const initialName = myError.name;
  myError.code = 'FHQWHGADS';
  assert.strictEqual(myError.code, 'FHQWHGADS');
  assert.strictEqual(myError.name, initialName);
  assert.deepStrictEqual(Object.keys(myError), ['code']);
  assert.ok(myError.name.includes('ERR_TLS_HANDSHAKE_TIMEOUT'));
  assert.ok(!myError.name.includes('FHQWHGADS'));
}

// Test that `name` is mutable and that changing it alters `toString()` but not
// `console.log()` results, which is the behavior of `Error` objects in the
// browser. Note that `name` becomes enumerable after being assigned.
{
  const myError = new errors.Error('ERR_TLS_HANDSHAKE_TIMEOUT');
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
  const myError = new errors.Error('ERR_TLS_HANDSHAKE_TIMEOUT');
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
