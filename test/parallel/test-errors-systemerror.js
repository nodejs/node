// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const errors = require('internal/errors');
const { AssertionError } = require('assert');

const { E, SystemError } = errors;

common.expectsError(
  () => { throw new errors.SystemError(); },
  {
    code: 'ERR_ASSERTION',
    type: AssertionError,
    message: 'An invalid error message key was used: undefined.'
  }
);

E('ERR_TEST', 'custom message', SystemError);
const { ERR_TEST } = errors.codes;

{
  const ctx = {
    code: 'ETEST',
    message: 'code message',
    syscall: 'syscall_test',
    path: '/str',
    dest: '/str2'
  };

  common.expectsError(
    () => { throw new ERR_TEST(ctx); },
    {
      code: 'ERR_TEST',
      type: SystemError,
      message: 'custom message: syscall_test returns ETEST (code message)' +
               ' /str => /str2',
      info: ctx
    }
  );
}

{
  const ctx = {
    code: 'ETEST',
    message: 'code message',
    syscall: 'syscall_test',
    path: Buffer.from('/buf'),
    dest: '/str2'
  };
  common.expectsError(
    () => { throw new ERR_TEST(ctx); },
    {
      code: 'ERR_TEST',
      type: SystemError,
      message: 'custom message: syscall_test returns ETEST (code message)' +
               ' /buf => /str2',
      info: ctx
    }
  );
}

{
  const ctx = {
    code: 'ETEST',
    message: 'code message',
    syscall: 'syscall_test',
    path: Buffer.from('/buf'),
    dest: Buffer.from('/buf2')
  };
  common.expectsError(
    () => { throw new ERR_TEST(ctx); },
    {
      code: 'ERR_TEST',
      type: SystemError,
      message: 'custom message: syscall_test returns ETEST (code message)' +
               ' /buf => /buf2',
      info: ctx
    }
  );
}

{
  const ctx = {
    code: 'ERR',
    errno: 123,
    message: 'something happened',
    syscall: 'syscall_test',
    path: Buffer.from('a'),
    dest: Buffer.from('b')
  };
  const err = new ERR_TEST(ctx);
  assert.strictEqual(err.info, ctx);
  assert.strictEqual(err.code, 'ERR_TEST');
  err.code = 'test';
  assert.strictEqual(err.code, 'test');

  // Test legacy properties. These shouldn't be used anymore
  // but let us make sure they still work
  assert.strictEqual(err.errno, 123);
  assert.strictEqual(err.syscall, 'syscall_test');
  assert.strictEqual(err.path, 'a');
  assert.strictEqual(err.dest, 'b');

  // Make sure it's mutable
  err.code = 'test';
  err.errno = 321;
  err.syscall = 'test';
  err.path = 'path';
  err.dest = 'path';

  assert.strictEqual(err.errno, 321);
  assert.strictEqual(err.syscall, 'test');
  assert.strictEqual(err.path, 'path');
  assert.strictEqual(err.dest, 'path');
}
