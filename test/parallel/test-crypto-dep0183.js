'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

common.expectWarning({
  DeprecationWarning: {
    DEP0183: 'OpenSSL engine-based APIs are deprecated.',
  },
});

assert.throws(
  () => crypto.setEngine('nodejs-test-invalid-engine'),
  (err) => {
    return err.code === 'ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED' ||
           err.code === 'ERR_CRYPTO_ENGINE_UNKNOWN';
  },
);
