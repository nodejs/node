// Flags: --expose-internals
'use strict';

const common = require('../common');
const { strictEqual } = require('assert');
const { NghttpError } = require('internal/http2/util');

common.expectsError(() => {
  const err = new NghttpError(-501);
  strictEqual(err.errno, -501);
  throw err;
}, {
  code: 'ERR_HTTP2_ERROR',
  type: NghttpError,
  message: 'Invalid argument'
});
