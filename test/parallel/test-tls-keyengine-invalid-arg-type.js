'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

assert.throws(
  () => {
    tls.createSecureContext({ privateKeyEngine: 0,
                              privateKeyIdentifier: 'key' });
  },
  { code: 'ERR_INVALID_ARG_TYPE',
    message: / Received type number \(0\)$/ });

assert.throws(
  () => {
    tls.createSecureContext({ privateKeyEngine: 'engine',
                              privateKeyIdentifier: 0 });
  },
  { code: 'ERR_INVALID_ARG_TYPE',
    message: / Received type number \(0\)$/ });
