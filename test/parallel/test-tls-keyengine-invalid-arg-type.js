'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');

common.expectsError(
  () => {
    tls.createSecureContext({ privateKeyEngine: 0,
                              privateKeyIdentifier: 'key' });
  },
  { code: 'ERR_INVALID_ARG_TYPE',
    message: / Received type number \(0\)$/ });

common.expectsError(
  () => {
    tls.createSecureContext({ privateKeyEngine: 'engine',
                              privateKeyIdentifier: 0 });
  },
  { code: 'ERR_INVALID_ARG_TYPE',
    message: / Received type number \(0\)$/ });
