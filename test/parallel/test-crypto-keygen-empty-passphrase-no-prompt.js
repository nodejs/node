'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  createPrivateKey,
  generateKeyPair,
} = require('crypto');
const {
  hasFIPS,
  testSignVerify,
  hasOpenSSL,
} = require('../common/crypto');

const fips4 = hasFIPS(4);

// Passing an empty passphrase string should not cause OpenSSL's default
// passphrase prompt in the terminal.
// See https://github.com/nodejs/node/issues/35898.
for (const type of ['pkcs1', 'pkcs8']) {
  generateKeyPair('rsa', {
    modulusLength: hasFIPS(3) ? 2048 : 1024,
    privateKeyEncoding: {
      type,
      format: 'pem',
      cipher: 'aes-256-cbc',
      passphrase: ''
    }
  }, common.mustCall((err, publicKey, privateKey) => {
    if (hasFIPS(3) && type === 'pkcs1') {
      assert.strictEqual(err?.code, 'ERR_OSSL_EVP_UNSUPPORTED');
      return;
    }
    if (fips4) {
      assert.strictEqual(
        err?.code,
        'ERR_OSSL_PASSWORD_STRENGTH_TOO_WEAK',
      );
      return;
    }
    assert.ifError(err);
    assert.strictEqual(publicKey.type, 'public');

    for (const passphrase of ['', Buffer.alloc(0)]) {
      const privateKeyObject = createPrivateKey({
        passphrase,
        key: privateKey
      });
      assert.strictEqual(privateKeyObject.asymmetricKeyType, 'rsa');
    }

    // Encrypting with an empty passphrase is not the same as not encrypting
    // the key, and not specifying a passphrase should fail when decoding it.
    assert.throws(() => {
      return testSignVerify(publicKey, privateKey);
    }, hasOpenSSL(3) ? {
      name: 'Error',
      code: 'ERR_OSSL_CRYPTO_INTERRUPTED_OR_CANCELLED',
      message: 'error:07880109:common libcrypto routines::interrupted or cancelled'
    } : {
      name: 'TypeError',
      code: 'ERR_MISSING_PASSPHRASE',
      message: 'Passphrase required for encrypted key'
    });
  }));
}
