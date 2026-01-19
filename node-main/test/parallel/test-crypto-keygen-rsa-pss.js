'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  constants,
  generateKeyPair,
} = require('crypto');
const {
  testEncryptDecrypt,
  testSignVerify,
} = require('../common/crypto');

// Test RSA-PSS.
{
  generateKeyPair('rsa-pss', {
    modulusLength: 512,
    saltLength: 16,
    hashAlgorithm: 'sha256',
    mgf1HashAlgorithm: 'sha256'
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(publicKey.asymmetricKeyType, 'rsa-pss');
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
      modulusLength: 512,
      publicExponent: 65537n,
      hashAlgorithm: 'sha256',
      mgf1HashAlgorithm: 'sha256',
      saltLength: 16
    });

    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(privateKey.asymmetricKeyType, 'rsa-pss');
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
      modulusLength: 512,
      publicExponent: 65537n,
      hashAlgorithm: 'sha256',
      mgf1HashAlgorithm: 'sha256',
      saltLength: 16
    });

    // Unlike RSA, RSA-PSS does not allow encryption.
    assert.throws(() => {
      testEncryptDecrypt(publicKey, privateKey);
    }, /operation not supported for this keytype/);

    // RSA-PSS also does not permit signing with PKCS1 padding.
    assert.throws(() => {
      testSignVerify({
        key: publicKey,
        padding: constants.RSA_PKCS1_PADDING
      }, {
        key: privateKey,
        padding: constants.RSA_PKCS1_PADDING
      });
    }, /illegal or unsupported padding mode/);

    // The padding should correctly default to RSA_PKCS1_PSS_PADDING now.
    testSignVerify(publicKey, privateKey);
  }));
}
