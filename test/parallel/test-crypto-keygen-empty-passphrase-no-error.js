'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// Passing an empty passphrase string should not throw ERR_OSSL_CRYPTO_MALLOC_FAILURE even on OpenSSL 3.
// Regression test for https://github.com/nodejs/node/issues/41428.
generateKeyPair('rsa', {
  modulusLength: 1024,
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
}, common.mustSucceed((publicKey, privateKey) => {
  assert.strictEqual(typeof publicKey, 'string');
  assert.strictEqual(typeof privateKey, 'string');
}));
