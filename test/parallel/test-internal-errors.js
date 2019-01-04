// Flags: --expose-internals
'use strict';
const common = require('../common');
const {
  hijackStdout,
  restoreStdout,
} = require('../common/hijackstdio');

const { internalBinding } = require('internal/test/binding');
const assert = require('assert');
const errors = require('internal/errors');

// Turn off ANSI color formatting for this test file.
const { inspect } = require('util');
inspect.defaultOptions.colors = false;

errors.E('TEST_ERROR_1', 'Error for testing purposes: %s',
         Error, TypeError, RangeError);
errors.E('TEST_ERROR_2', (a, b) => `${a} ${b}`, Error);

{
  const err = new errors.codes.TEST_ERROR_1('test');
  assert(err instanceof Error);
  assert.strictEqual(err.name, 'Error [TEST_ERROR_1]');
  assert.strictEqual(err.message, 'Error for testing purposes: test');
  assert.strictEqual(err.code, 'TEST_ERROR_1');
}

{
  const err = new errors.codes.TEST_ERROR_1.TypeError('test');
  assert(err instanceof TypeError);
  assert.strictEqual(err.name, 'TypeError [TEST_ERROR_1]');
  assert.strictEqual(err.message, 'Error for testing purposes: test');
  assert.strictEqual(err.code, 'TEST_ERROR_1');
}

{
  const err = new errors.codes.TEST_ERROR_1.RangeError('test');
  assert(err instanceof RangeError);
  assert.strictEqual(err.name, 'RangeError [TEST_ERROR_1]');
  assert.strictEqual(err.message, 'Error for testing purposes: test');
  assert.strictEqual(err.code, 'TEST_ERROR_1');
}

{
  const err = new errors.codes.TEST_ERROR_2('abc', 'xyz');
  assert(err instanceof Error);
  assert.strictEqual(err.name, 'Error [TEST_ERROR_2]');
  assert.strictEqual(err.message, 'abc xyz');
  assert.strictEqual(err.code, 'TEST_ERROR_2');
}

{
  assert.throws(
    () => new errors.codes.TEST_ERROR_1(),
    {
      message: 'Code: TEST_ERROR_1; The provided arguments ' +
               'length (0) does not match the required ones (1).'
    }
  );
}

// Tests for common.expectsError
common.expectsError(() => {
  throw new errors.codes.TEST_ERROR_1.TypeError('a');
}, { code: 'TEST_ERROR_1' });
common.expectsError(() => {
  throw new errors.codes.TEST_ERROR_1.TypeError('a');
}, { code: 'TEST_ERROR_1',
     type: TypeError,
     message: /^Error for testing/ });
common.expectsError(() => {
  throw new errors.codes.TEST_ERROR_1.TypeError('a');
}, { code: 'TEST_ERROR_1', type: TypeError });
common.expectsError(() => {
  throw new errors.codes.TEST_ERROR_1.TypeError('a');
}, {
  code: 'TEST_ERROR_1',
  type: TypeError,
  message: 'Error for testing purposes: a'
});

common.expectsError(() => {
  common.expectsError(() => {
    throw new errors.codes.TEST_ERROR_1.TypeError('a');
  }, { code: 'TEST_ERROR_1', type: RangeError });
}, {
  code: 'ERR_ASSERTION',
  message: /\+   type: \[Function: TypeError]\n-   type: \[Function: RangeError]/
});

common.expectsError(() => {
  common.expectsError(() => {
    throw new errors.codes.TEST_ERROR_1.TypeError('a');
  }, { code: 'TEST_ERROR_1',
       type: TypeError,
       message: /^Error for testing 2/ });
}, {
  code: 'ERR_ASSERTION',
  type: assert.AssertionError,
  message: /\+   message: 'Error for testing purposes: a',\n-   message: \/\^Error/
});

// Test ERR_INVALID_FD_TYPE
assert.strictEqual(errors.getMessage('ERR_INVALID_FD_TYPE', ['a']),
                   'Unsupported fd type: a');

// Test ERR_INVALID_URL_SCHEME
assert.strictEqual(errors.getMessage('ERR_INVALID_URL_SCHEME', ['file']),
                   'The URL must be of scheme file');
assert.strictEqual(errors.getMessage('ERR_INVALID_URL_SCHEME', [['file']]),
                   'The URL must be of scheme file');
assert.strictEqual(errors.getMessage('ERR_INVALID_URL_SCHEME',
                                     [['http', 'ftp']]),
                   'The URL must be one of scheme http or ftp');
assert.strictEqual(errors.getMessage('ERR_INVALID_URL_SCHEME',
                                     [['a', 'b', 'c']]),
                   'The URL must be one of scheme a, b, or c');
common.expectsError(
  () => errors.getMessage('ERR_INVALID_URL_SCHEME', [[]]),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: /^At least one expected value needs to be specified$/
  });

// Test ERR_MISSING_ARGS
assert.strictEqual(errors.getMessage('ERR_MISSING_ARGS', ['name']),
                   'The "name" argument must be specified');
assert.strictEqual(errors.getMessage('ERR_MISSING_ARGS', ['name', 'value']),
                   'The "name" and "value" arguments must be specified');
assert.strictEqual(errors.getMessage('ERR_MISSING_ARGS', ['a', 'b', 'c']),
                   'The "a", "b", and "c" arguments must be specified');

// Test ERR_SOCKET_BAD_PORT
assert.strictEqual(
  errors.getMessage('ERR_SOCKET_BAD_PORT', [0]),
  'Port should be >= 0 and < 65536. Received 0.');

// Test ERR_TLS_CERT_ALTNAME_INVALID
assert.strictEqual(
  errors.getMessage('ERR_TLS_CERT_ALTNAME_INVALID', ['altname']),
  'Hostname/IP does not match certificate\'s altnames: altname');

assert.strictEqual(
  errors.getMessage('ERR_INVALID_PROTOCOL', ['bad protocol', 'http']),
  'Protocol "bad protocol" not supported. Expected "http"'
);

assert.strictEqual(
  errors.getMessage('ERR_HTTP_HEADERS_SENT', ['render']),
  'Cannot render headers after they are sent to the client'
);

assert.strictEqual(
  errors.getMessage('ERR_INVALID_HTTP_TOKEN', ['Method', 'foo']),
  'Method must be a valid HTTP token ["foo"]'
);

assert.strictEqual(
  errors.getMessage('ERR_OUT_OF_RANGE', ['A', 'some values', 'B']),
  'The value of "A" is out of range. It must be some values. Received B'
);

assert.strictEqual(
  errors.getMessage('ERR_UNESCAPED_CHARACTERS', ['Request path']),
  'Request path contains unescaped characters'
);

// Test ERR_DNS_SET_SERVERS_FAILED
assert.strictEqual(
  errors.getMessage('ERR_DNS_SET_SERVERS_FAILED', ['err', 'servers']),
  'c-ares failed to set servers: "err" [servers]');

// Test ERR_ENCODING_NOT_SUPPORTED
assert.strictEqual(
  errors.getMessage('ERR_ENCODING_NOT_SUPPORTED', ['enc']),
  'The "enc" encoding is not supported');

// Test error messages for async_hooks
assert.strictEqual(
  errors.getMessage('ERR_ASYNC_CALLBACK', ['init']),
  'init must be a function');
assert.strictEqual(
  errors.getMessage('ERR_ASYNC_TYPE', [{}]),
  'Invalid name for async "type": [object Object]');
assert.strictEqual(
  errors.getMessage('ERR_INVALID_ASYNC_ID', ['asyncId', undefined]),
  'Invalid asyncId value: undefined');

{
  const { kMaxLength } = internalBinding('buffer');
  const error = new errors.codes.ERR_BUFFER_TOO_LARGE();
  assert.strictEqual(
    error.message,
    `Cannot create a Buffer larger than 0x${kMaxLength.toString(16)} bytes`
  );
}

{
  const error = new errors.codes.ERR_INVALID_ARG_VALUE('foo', '\u0000bar');
  assert.strictEqual(
    error.message,
    'The argument \'foo\' is invalid. Received \'\\u0000bar\''
  );
}

{
  const error = new errors.codes.ERR_INVALID_ARG_VALUE(
    'foo', { a: 1 }, 'must have property \'b\''
  );
  assert.strictEqual(
    error.message,
    'The argument \'foo\' must have property \'b\'. Received { a: 1 }'
  );
}

// Test that `code` property is mutable and that changing it does not change the
// name.
{
  const myError = new errors.codes.ERR_TLS_HANDSHAKE_TIMEOUT();
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
  const myError = new errors.codes.ERR_TLS_HANDSHAKE_TIMEOUT();
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
  hijackStdout((data) => { initialConsoleLog += data; });
  const myError = new errors.codes.ERR_TLS_HANDSHAKE_TIMEOUT();
  assert.deepStrictEqual(Object.keys(myError), []);
  const initialToString = myError.toString();
  console.log(myError);
  assert.notStrictEqual(initialConsoleLog, '');

  restoreStdout();

  let subsequentConsoleLog = '';
  hijackStdout((data) => { subsequentConsoleLog += data; });
  myError.message = 'Fhqwhgads';
  assert.deepStrictEqual(Object.keys(myError), []);
  assert.notStrictEqual(myError.toString(), initialToString);
  console.log(myError);
  assert.strictEqual(subsequentConsoleLog, initialConsoleLog);

  restoreStdout();
}

{
  const error = new errors.codes.ERR_WORKER_INVALID_EXEC_ARGV(
    ['--foo, --bar']
  );
  assert.strictEqual(
    error.message,
    'Initiated Worker with invalid execArgv flags: --foo, --bar'
  );
}
