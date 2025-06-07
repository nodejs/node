// Flags: --no-warnings
'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const crypto = require('crypto');

common.expectWarning(
  'DeprecationWarning',
  'ecdh.setPublicKey() is deprecated.', 'DEP0031');

const ec = crypto.createECDH('secp256k1');
try {
  // This will throw but we don't care about the error,
  // we just want to verify that the deprecation warning
  // is emitter.
  ec.setPublicKey(Buffer.from([123]));
} catch {
  // Intentionally ignore the error
}
