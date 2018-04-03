'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');

{
  common.expectsError(
    () => { tls.createSecureContext({ clientCertEngine: 0 }); },
    { code: 'ERR_INVALID_ARG_TYPE',
      message: / Received type number$/ });
}
