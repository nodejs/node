'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const {
  isBoringSSL,
  hasOpenSSL,
  hasFIPS,
} = require('../common/crypto');

if (isBoringSSL)
  common.skip('BoringSSL does not support arbitrary RSA modulus length ' +
              'or RSA-PSS/DSA key generation');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

const fips3 = hasFIPS(3);

// This tests check that generateKeyPair returns correct bit length in
// KeyObject's asymmetricKeyDetails.
// https://github.com/nodejs/node/issues/46102#issuecomment-1372153541
{
  generateKeyPair('rsa', {
    modulusLength: 513,
  }, common.mustCall((err, publicKey, privateKey) => {
    if (fips3) {
      assert.strictEqual(err?.message, 'error:020000AE:rsa routines::invalid modulus');
      return;
    }
    assert.ifError(err);
    assert.strictEqual(privateKey.asymmetricKeyDetails.modulusLength, 513);
    assert.strictEqual(publicKey.asymmetricKeyDetails.modulusLength, 513);
  }));

  generateKeyPair('rsa-pss', {
    modulusLength: 513,
  }, common.mustCall((err, publicKey, privateKey) => {
    if (fips3) {
      assert.strictEqual(err?.message, 'error:020000AE:rsa routines::invalid modulus');
      return;
    }
    assert.ifError(err);
    assert.strictEqual(privateKey.asymmetricKeyDetails.modulusLength, 513);
    assert.strictEqual(publicKey.asymmetricKeyDetails.modulusLength, 513);
  }));

  if (hasOpenSSL(3)) {
    generateKeyPair('dsa', {
      modulusLength: 2049,
      divisorLength: 256,
    }, common.mustCall((err, publicKey, privateKey) => {
      if (fips3) {
        assert.strictEqual(err?.message, 'error:05000072:dsa routines::bad ffc parameters');
        return;
      }
      assert.ifError(err);
      assert.strictEqual(privateKey.asymmetricKeyDetails.modulusLength, 2049);
      assert.strictEqual(publicKey.asymmetricKeyDetails.modulusLength, 2049);
    }));
  }
}
