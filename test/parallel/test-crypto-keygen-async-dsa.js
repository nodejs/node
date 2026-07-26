'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const {
  isBoringSSL,
  assertApproximateSize,
  testSignVerify,
  spkiExp,
} = require('../common/crypto');

if (isBoringSSL)
  common.skip('not supported by BoringSSL');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');
// Test async DSA key generation.
{
  const privateKeyEncoding = {
    type: 'pkcs8',
    format: 'der'
  };

  generateKeyPair('dsa', {
    modulusLength: 2048,
    divisorLength: 256,
    publicKeyEncoding: {
      type: 'spki',
      format: 'pem'
    },
    privateKeyEncoding: {
      cipher: 'aes-128-cbc',
      passphrase: 'password',
      ...privateKeyEncoding
    }
  }, common.mustSucceed((publicKey, privateKeyDER) => {
    assert.strictEqual(typeof publicKey, 'string');
    assert.match(publicKey, spkiExp);
    // The private key is DER-encoded.
    assert(Buffer.isBuffer(privateKeyDER));

    assertApproximateSize(publicKey, 1194);
    assertApproximateSize(privateKeyDER, 721);

    // Since the private key is encrypted, signing shouldn't work anymore.
    assert.throws(() => {
      return testSignVerify(publicKey, {
        key: privateKeyDER,
        ...privateKeyEncoding
      });
    }, {
      name: 'TypeError',
      code: 'ERR_MISSING_PASSPHRASE',
      message: 'Passphrase required for encrypted key'
    });

    // Signing should work with the correct password.
    testSignVerify(publicKey, {
      key: privateKeyDER,
      ...privateKeyEncoding,
      passphrase: 'password'
    });
  }));
}
