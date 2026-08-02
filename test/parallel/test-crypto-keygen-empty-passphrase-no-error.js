'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');
const { hasFIPS } = require('../common/crypto');

const fips4 = hasFIPS(4);

// Passing an empty passphrase string should not throw ERR_OSSL_CRYPTO_MALLOC_FAILURE even on OpenSSL 3.
// Regression test for https://github.com/nodejs/node/issues/41428.
generateKeyPair('rsa', {
  modulusLength: hasFIPS(3) ? 2048 : 1024,
  publicKeyEncoding: {
    type: 'spki',
    format: 'pem'
  },
  privateKeyEncoding: {
    type: 'pkcs8',
    format: 'pem',
    cipher: 'aes-256-cbc',
    passphrase: ''
  }
}, common.mustCall((err, publicKey, privateKey) => {
  if (fips4) {
    assert.strictEqual(err?.code, 'ERR_OSSL_PASSWORD_STRENGTH_TOO_WEAK');
    return;
  }
  assert.ifError(err);
  assert.strictEqual(typeof publicKey, 'string');
  assert.strictEqual(typeof privateKey, 'string');
}));
