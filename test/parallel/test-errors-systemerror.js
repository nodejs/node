// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const errors = require('internal/errors');

common.expectsError(
  () => { throw new errors.SystemError(); },
  {
    code: 'ERR_SYSTEM_ERROR',
    type: errors.SystemError,
    message: 'A system error occurred'
  }
);

common.expectsError(
  () => { throw new errors.SystemError({}); },
  {
    code: 'ERR_SYSTEM_ERROR',
    type: errors.SystemError,
    message: 'A system error occurred'
  }
);

common.expectsError(
  () => { throw new errors.SystemError(null); },
  {
    code: 'ERR_SYSTEM_ERROR',
    type: errors.SystemError,
    message: 'A system error occurred'
  }
);

common.expectsError(
  () => { throw new errors.SystemError({ code: 'ERR' }); },
  {
    code: 'ERR_SYSTEM_ERROR',
    type: errors.SystemError,
    message: 'A system error occurred: ERR'
  }
);

{
  const ctx = {
    code: 'ERR',
    syscall: 'foo'
  };
  common.expectsError(
    () => { throw new errors.SystemError(ctx); },
    {
      code: 'ERR_SYSTEM_ERROR',
      type: errors.SystemError,
      message: 'A system error occurred: ERR [foo]'
    }
  );
}

{
  const ctx = {
    code: 'ERR',
    syscall: 'foo',
    path: Buffer.from('a')
  };
  common.expectsError(
    () => { throw new errors.SystemError(ctx); },
    {
      code: 'ERR_SYSTEM_ERROR',
      type: errors.SystemError,
      message: 'A system error occurred: ERR [foo]: a'
    }
  );
}

{
  const ctx = {
    code: 'ERR',
    syscall: 'foo',
    path: Buffer.from('a'),
    dest: Buffer.from('b')
  };
  common.expectsError(
    () => { throw new errors.SystemError(ctx); },
    {
      code: 'ERR_SYSTEM_ERROR',
      type: errors.SystemError,
      message: 'A system error occurred: ERR [foo]: a => b'
    }
  );
}

{
  const ctx = {
    syscall: 'foo',
    path: Buffer.from('a'),
    dest: Buffer.from('b')
  };
  common.expectsError(
    () => { throw new errors.SystemError(ctx); },
    {
      code: 'ERR_SYSTEM_ERROR',
      type: errors.SystemError,
      message: 'A system error occurred: [foo]: a => b'
    }
  );
}

{
  const ctx = {
    path: Buffer.from('a'),
    dest: Buffer.from('b')
  };
  common.expectsError(
    () => { throw new errors.SystemError(ctx); },
    {
      code: 'ERR_SYSTEM_ERROR',
      type: errors.SystemError,
      message: 'A system error occurred: a => b'
    }
  );
}

{
  const ctx = {
    code: 'ERR',
    message: 'something happened',
    syscall: 'foo',
    path: Buffer.from('a'),
    dest: Buffer.from('b')
  };
  common.expectsError(
    () => { throw new errors.SystemError(ctx); },
    {
      code: 'ERR_SYSTEM_ERROR',
      type: errors.SystemError,
      message: 'something happened: ERR [foo]: a => b'
    }
  );
}

{
  const ctx = {
    code: 'ERR_ASSERTION'
  };
  common.expectsError(
    () => { throw new errors.SystemError(ctx); },
    {
      code: 'ERR_ASSERTION',
      type: errors.SystemError
    }
  );
}

{
  const ctx = {
    code: 'ERR',
    errno: 123,
    message: 'something happened',
    syscall: 'foo',
    path: Buffer.from('a'),
    dest: Buffer.from('b')
  };
  const err = new errors.SystemError(ctx);
  assert.strictEqual(err.info, ctx);
  assert.strictEqual(err.code, 'ERR_SYSTEM_ERROR');
  err.code = 'test';
  assert.strictEqual(err.code, 'test');

  // Test legacy properties. These shouldn't be used anymore
  // but let us make sure they still work
  assert.strictEqual(err.errno, 123);
  assert.strictEqual(err.syscall, 'foo');
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
