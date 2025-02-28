'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// RFC 8017, A.2.3.: "For a given hashAlgorithm, the default value of
// saltLength is the octet length of the hash value."
{
  generateKeyPair('rsa-pss', {
    modulusLength: 512,
    hashAlgorithm: 'sha512'
  }, common.mustSucceed((publicKey, privateKey) => {
    const expectedKeyDetails = {
      modulusLength: 512,
      publicExponent: 65537n,
      hashAlgorithm: 'sha512',
      mgf1HashAlgorithm: 'sha512',
      saltLength: 64
    };
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, expectedKeyDetails);
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, expectedKeyDetails);
  }));

  // It is still possible to explicitly set saltLength to 0.
  generateKeyPair('rsa-pss', {
    modulusLength: 512,
    hashAlgorithm: 'sha512',
    saltLength: 0
  }, common.mustSucceed((publicKey, privateKey) => {
    const expectedKeyDetails = {
      modulusLength: 512,
      publicExponent: 65537n,
      hashAlgorithm: 'sha512',
      mgf1HashAlgorithm: 'sha512',
      saltLength: 0
    };
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, expectedKeyDetails);
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, expectedKeyDetails);
  }));
}
