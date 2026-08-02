'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (process.features.openssl_is_boringssl)
  common.skip('BoringSSL does not support RSA-PSS key pair generation');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');
const { hasFIPS } = require('../common/crypto');

// 'rsa-pss' should not add a RSASSA-PSS-params sequence by default.
// Regression test for: https://github.com/nodejs/node/issues/39936
{
  const modulusLength = hasFIPS(3) ? 2048 : 512;
  generateKeyPair('rsa-pss', {
    modulusLength
  }, common.mustSucceed((publicKey, privateKey) => {
    const expectedKeyDetails = {
      modulusLength,
      publicExponent: 65537n
    };
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, expectedKeyDetails);
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, expectedKeyDetails);

    // To allow backporting the fix to versions that do not support
    // asymmetricKeyDetails for RSA-PSS params, also verify that the exported
    // AlgorithmIdentifier member of the SubjectPublicKeyInfo has the expected
    // length of 11 bytes (as opposed to > 11 bytes if node added params).
    const spki = publicKey.export({ format: 'der', type: 'spki' });
    assert.strictEqual(
      spki[3], hasFIPS(3) ? 32 : 11, spki.toString('hex'));
  }));
}
