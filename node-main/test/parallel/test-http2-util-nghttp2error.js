// Flags: --expose-internals
'use strict';

require('../common');
const { strictEqual, throws } = require('assert');
const { NghttpError } = require('internal/http2/util');

throws(() => {
  const err = new NghttpError(-501);
  strictEqual(err.errno, -501);
  throw err;
}, {
  code: 'ERR_HTTP2_ERROR',
  constructor: NghttpError,
  message: 'Invalid argument'
});

// Should convert the NghttpError object to string properly
{
  const err = new NghttpError(401);
  strictEqual(err.toString(), 'Error [ERR_HTTP2_ERROR]: Unknown error code');
}
