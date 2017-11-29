'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

{
  assert.throws(
    () => { tls.createSecureContext({ clientCertEngine: 0 }); },
    common.expectsError({ code: 'ERR_INVALID_ARG_TYPE',
                          message: / Received type number$/ }));
}
