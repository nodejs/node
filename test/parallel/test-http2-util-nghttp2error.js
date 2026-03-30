// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { NghttpError } = require('internal/http2/util');

assert.throws(() => {
  const err = new NghttpError(-501);
  assert.strictEqual(err.errno, -501);
  throw err;
}, {
  code: 'ERR_HTTP2_ERROR',
  constructor: NghttpError,
  message: 'Invalid argument'
});

// Should convert the NghttpError object to string properly
{
  const err = new NghttpError(401);
  assert.strictEqual(err.toString(), 'Error [ERR_HTTP2_ERROR]: Unknown error code');
}
