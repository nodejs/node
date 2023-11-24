'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');
const {
  assertApproximateSize,
  testEncryptDecrypt,
  testSignVerify,
} = require('../common/crypto');

// Test async RSA key generation with an encrypted private key, but encoded as DER.
{
  generateKeyPair('rsa', {
    publicExponent: 0x10001,
    modulusLength: 512,
    publicKeyEncoding: {
      type: 'pkcs1',
      format: 'der'
    },
    privateKeyEncoding: {
      type: 'pkcs8',
      format: 'der',
      cipher: 'aes-256-cbc',
      passphrase: 'secret'
    }
  }, common.mustSucceed((publicKeyDER, privateKeyDER) => {
    assert(Buffer.isBuffer(publicKeyDER));
    assertApproximateSize(publicKeyDER, 74);

    assert(Buffer.isBuffer(privateKeyDER));

    // Since the private key is encrypted, signing shouldn't work anymore.
    const publicKey = {
      key: publicKeyDER,
      type: 'pkcs1',
      format: 'der',
    };
    assert.throws(() => {
      testSignVerify(publicKey, {
        key: privateKeyDER,
        format: 'der',
        type: 'pkcs8'
      });
    }, {
      name: 'TypeError',
      code: 'ERR_MISSING_PASSPHRASE',
      message: 'Passphrase required for encrypted key'
    });

    // Signing should work with the correct password.

    const privateKey = {
      key: privateKeyDER,
      format: 'der',
      type: 'pkcs8',
      passphrase: 'secret'
    };
    testEncryptDecrypt(publicKey, privateKey);
    testSignVerify(publicKey, privateKey);
  }));
}
