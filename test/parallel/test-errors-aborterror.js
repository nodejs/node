// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { AbortError } = require('internal/errors');

{
  const err = new AbortError();
  assert.strictEqual(err.message, 'The operation was aborted');
  assert.strictEqual(err.cause, undefined);
}

{
  const cause = new Error('boom');
  const err = new AbortError('bang', { cause });
  assert.strictEqual(err.message, 'bang');
  assert.strictEqual(err.cause, cause);
}

{
  assert.throws(() => new AbortError('', false), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => new AbortError('', ''), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
}
