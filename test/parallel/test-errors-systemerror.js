// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { E, SystemError, codes } = require('internal/errors');

assert.throws(
  () => { new SystemError(); },
  {
    name: 'TypeError',
    message: 'Cannot read property \'match\' of undefined'
  }
);

E('ERR_TEST', 'custom message', SystemError);
const { ERR_TEST } = codes;

{
  const ctx = {
    code: 'ETEST',
    message: 'code message',
    syscall: 'syscall_test',
    path: '/str',
    dest: '/str2'
  };

  assert.throws(
    () => { throw new ERR_TEST(ctx); },
    {
      code: 'ERR_TEST',
      name: 'SystemError',
      message: 'custom message: syscall_test returned ETEST (code message)' +
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
  assert.throws(
    () => { throw new ERR_TEST(ctx); },
    {
      code: 'ERR_TEST',
      name: 'SystemError',
      message: 'custom message: syscall_test returned ETEST (code message)' +
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
  assert.throws(
    () => { throw new ERR_TEST(ctx); },
    {
      code: 'ERR_TEST',
      name: 'SystemError',
      message: 'custom message: syscall_test returned ETEST (code message)' +
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

{
  const ctx = {
    code: 'ERR_TEST',
    message: 'Error occurred',
    syscall: 'syscall_test'
  };
  assert.throws(
    () => {
      const err = new ERR_TEST(ctx);
      err.name = 'Foobar';
      throw err;
    },
    {
      code: 'ERR_TEST',
      name: 'Foobar',
      message: 'custom message: syscall_test returned ERR_TEST ' +
               '(Error occurred)',
      info: ctx
    }
  );
}
