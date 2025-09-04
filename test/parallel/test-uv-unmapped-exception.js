// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { UVException, UVExceptionWithHostPort } = require('internal/errors');
const { _exceptionWithHostPort } = require('util');
const { internalBinding } = require('internal/test/binding');
const uv = internalBinding('uv');

{
  const exception = new UVException({ errno: 100, syscall: 'open' });

  assert.strictEqual(exception.message, 'UNKNOWN: unknown error, open');
  assert.strictEqual(exception.errno, 100);
  assert.strictEqual(exception.syscall, 'open');
  assert.strictEqual(exception.code, 'UNKNOWN');
}

{
  const exception = new UVExceptionWithHostPort(100, 'listen', '127.0.0.1', 80);

  assert.strictEqual(exception.message,
                     'listen UNKNOWN: unknown error 127.0.0.1:80');
  assert.strictEqual(exception.code, 'UNKNOWN');
  assert.strictEqual(exception.errno, 100);
  assert.strictEqual(exception.syscall, 'listen');
  assert.strictEqual(exception.address, '127.0.0.1');
  assert.strictEqual(exception.port, 80);
}

{
  const err = _exceptionWithHostPort(uv.UV_EADDRINUSE, 'bind', '127.0.0.1', 8080);
  assert(err instanceof Error);
  assert.strictEqual(err.code, 'EADDRINUSE');
  assert.strictEqual(err.errno, uv.UV_EADDRINUSE);
  assert.strictEqual(err.syscall, 'bind');
  assert.strictEqual(err.address, '127.0.0.1');
  assert.strictEqual(err.port, 8080);
  assert.strictEqual(err.message, 'bind EADDRINUSE 127.0.0.1:8080');
}
