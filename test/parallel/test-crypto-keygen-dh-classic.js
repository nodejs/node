'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { isBoringSSL, hasFIPS } = require('../common/crypto');

if (isBoringSSL)
  common.skip('BoringSSL does not support DH key pair generation');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// Test classic Diffie-Hellman key generation.
{
  generateKeyPair('dh', {
    primeLength: hasFIPS(3) ? 2048 : 512
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(publicKey.asymmetricKeyType, 'dh');

    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(privateKey.asymmetricKeyType, 'dh');
  }));
}
