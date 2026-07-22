'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (process.features.openssl_is_boringssl)
  common.skip('BoringSSL does not support arbitrary RSA modulus length ' +
              'or RSA-PSS/DSA key generation');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');
const { hasOpenSSL3 } = require('../common/crypto');

// This tests check that generateKeyPair returns correct bit length in
// KeyObject's asymmetricKeyDetails.
// https://github.com/nodejs/node/issues/46102#issuecomment-1372153541
{
  generateKeyPair('rsa', {
    modulusLength: 513,
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(privateKey.asymmetricKeyDetails.modulusLength, 513);
    assert.strictEqual(publicKey.asymmetricKeyDetails.modulusLength, 513);
  }));

  generateKeyPair('rsa-pss', {
    modulusLength: 513,
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(privateKey.asymmetricKeyDetails.modulusLength, 513);
    assert.strictEqual(publicKey.asymmetricKeyDetails.modulusLength, 513);
  }));

  if (hasOpenSSL3) {
    generateKeyPair('dsa', {
      modulusLength: 2049,
      divisorLength: 256,
    }, common.mustSucceed((publicKey, privateKey) => {
      assert.strictEqual(privateKey.asymmetricKeyDetails.modulusLength, 2049);
      assert.strictEqual(publicKey.asymmetricKeyDetails.modulusLength, 2049);
    }));
  }
}
