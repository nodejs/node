// Flags: --expose-internals
'use strict';

require('../common');
const {
  strictEqual,
  throws,
} = require('assert');
const { AbortError } = require('internal/errors');

{
  const err = new AbortError();
  strictEqual(err.message, 'The operation was aborted');
  strictEqual(err.cause, undefined);
}

{
  const cause = new Error('boom');
  const err = new AbortError('bang', { cause });
  strictEqual(err.message, 'bang');
  strictEqual(err.cause, cause);
}

{
  throws(() => new AbortError('', false), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  throws(() => new AbortError('', ''), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
}
