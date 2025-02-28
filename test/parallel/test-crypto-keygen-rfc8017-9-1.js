'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// RFC 8017, 9.1.: "Assuming that the mask generation function is based on a
// hash function, it is RECOMMENDED that the hash function be the same as the
// one that is applied to the message."
{

  generateKeyPair('rsa-pss', {
    modulusLength: 512,
    hashAlgorithm: 'sha256',
    saltLength: 16
  }, common.mustSucceed((publicKey, privateKey) => {
    const expectedKeyDetails = {
      modulusLength: 512,
      publicExponent: 65537n,
      hashAlgorithm: 'sha256',
      mgf1HashAlgorithm: 'sha256',
      saltLength: 16
    };
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, expectedKeyDetails);
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, expectedKeyDetails);
  }));
}
