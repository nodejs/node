'use strict';

const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');

// Verify that passing an IP address the the servername option
// throws an error.
assert.throws(() => tls.connect({
  port: 1234,
  servername: '127.0.0.1',
}, common.mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
});
