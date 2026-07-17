'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (process.features.openssl_is_boringssl)
  common.skip('BoringSSL does not support DH key pair generation');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// Test classic Diffie-Hellman key generation.
{
  generateKeyPair('dh', {
    primeLength: 512
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(publicKey.asymmetricKeyType, 'dh');

    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(privateKey.asymmetricKeyType, 'dh');
  }));
}
