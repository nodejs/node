'use strict';

// This tests that the crypto error stack can be correctly converted.
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

common.expectWarning({
  DeprecationWarning: {
    DEP0183: 'OpenSSL engine-based APIs are deprecated.',
  },
});

assert.throws(() => {
  tls.createSecureContext({ clientCertEngine: 'x' });
}, (err) => {
  if (err.code === 'ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED')
    common.skip('OpenSSL dropped engine support');

  return err.name === 'Error' &&
         /could not load the shared library/.test(err.message) &&
         Array.isArray(err.opensslErrorStack) &&
        err.opensslErrorStack.length > 0;
});
