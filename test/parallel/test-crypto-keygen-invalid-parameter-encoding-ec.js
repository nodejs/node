'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const {
  generateKeyPairSync,
} = require('crypto');

{
  assert.throws(() => generateKeyPairSync('ec', {
    namedCurve: 'secp224r1',
    publicKeyEncoding: {
      format: 'jwk'
    },
    privateKeyEncoding: {
      format: 'jwk'
    }
  }), {
    name: 'Error',
    code: 'ERR_CRYPTO_JWK_UNSUPPORTED_CURVE',
    message: 'Unsupported JWK EC curve: secp224r1.'
  });
}
