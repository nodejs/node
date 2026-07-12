'use strict';

// Verify that privateDecrypt() does not leave an error on the
// openssl error stack that is visible to subsequent operations.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPairSync,
  publicEncrypt,
  privateDecrypt,
} = require('crypto');

const {
  hasOpenSSL,
  hasFIPS,
} = require('../common/crypto');

const fips3 = hasFIPS(3);
const fips4 = hasFIPS(4);
const pair = generateKeyPairSync('rsa', {
  modulusLength: fips3 ? 2048 : 512,
});

const expected = Buffer.from('shibboleth');
const options = fips3 ? { oaepHash: 'sha256' } : {};
const encrypted = publicEncrypt({ key: pair.publicKey, ...options }, expected);

const pkey = pair.privateKey.export({ type: 'pkcs1', format: 'pem' });
if (fips3) {
  assert.throws(() => pair.privateKey.export({
    type: 'pkcs1',
    format: 'pem',
    cipher: 'aes-128-cbc',
    passphrase: 'secret',
  }), {
    code: 'ERR_OSSL_EVP_UNSUPPORTED',
  });
}
if (fips4) {
  assert.throws(() => pair.privateKey.export({
    type: 'pkcs8',
    format: 'pem',
    cipher: 'aes-256-cbc',
    passphrase: 'secret',
  }), {
    code: 'ERR_OSSL_PASSWORD_STRENGTH_TOO_WEAK',
  });
}
const pkeyEncrypted =
  pair.privateKey.export({
    type: fips3 ? 'pkcs8' : 'pkcs1',
    format: 'pem',
    cipher: fips3 ? 'aes-256-cbc' : 'aes-128-cbc',
    passphrase: 'password',
  });

function decrypt(key) {
  const decrypted = privateDecrypt({ key, ...options }, encrypted);
  assert.deepStrictEqual(decrypted, expected);
}

decrypt(pkey);
assert.throws(() => decrypt(pkeyEncrypted), hasOpenSSL(3) ?
  { message: 'error:07880109:common libcrypto routines::interrupted or ' +
             'cancelled' } :
  { code: 'ERR_MISSING_PASSPHRASE' });
decrypt(pkey);  // Should not throw.
