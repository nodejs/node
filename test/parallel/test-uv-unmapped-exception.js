// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { uvException, uvExceptionWithHostPort } = require('internal/errors');

{
  const exception = uvException({ errno: 100, syscall: 'open' });

  assert.strictEqual(exception.message, 'UNKNOWN: unknown error, open');
  assert.strictEqual(exception.errno, 100);
  assert.strictEqual(exception.syscall, 'open');
  assert.strictEqual(exception.code, 'UNKNOWN');
}

{
  const exception = uvExceptionWithHostPort(100, 'listen', '127.0.0.1', 80);

  assert.strictEqual(exception.message,
                     'listen UNKNOWN: unknown error 127.0.0.1:80');
  assert.strictEqual(exception.code, 'UNKNOWN');
  assert.strictEqual(exception.errno, 100);
  assert.strictEqual(exception.syscall, 'listen');
  assert.strictEqual(exception.address, '127.0.0.1');
  assert.strictEqual(exception.port, 80);
}
