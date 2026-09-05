'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { isBoringSSL, hasFIPS } = require('../common/crypto');

if (isBoringSSL)
  common.skip('BoringSSL does not support DSA key pair generation');

const assert = require('assert');

const {
  generateKeyPairSync,
} = require('crypto');

const fips3 = hasFIPS(3);

// Test invalid parameter encoding.
{
  assert.throws(() => generateKeyPairSync('dsa', {
    modulusLength: 1024,
    publicKeyEncoding: {
      format: 'jwk'
    },
    privateKeyEncoding: {
      format: 'jwk'
    }
  }), {
    name: 'Error',
    code: fips3 ? 'ERR_OSSL_DSA_BAD_FFC_PARAMETERS' :
      'ERR_CRYPTO_JWK_UNSUPPORTED_KEY_TYPE',
    ...!fips3 && { message: 'Unsupported JWK Key Type.' },
  });
}
