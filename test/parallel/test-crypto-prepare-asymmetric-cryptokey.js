'use strict';

// Flags: --expose-internals

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');
const assert = require('node:assert');
const crypto = require('node:crypto');
const fixtures = require('../common/fixtures');
const {
  prepareAsymmetricKey,
  getCryptoKeyHandle,
  kConsumePublic,
  kConsumePrivate,
  kCreatePublic,
  kCreatePrivate,
} = require('internal/crypto/keys');

common.expectWarning({
  DeprecationWarning: {
    DEP0203: 'Passing a CryptoKey to node:crypto functions is deprecated.',
  },
});

const privateKeyObject = crypto.createPrivateKey(fixtures.readKey('ec_p256_private.pem'));
const algorithm = { name: 'ECDSA', namedCurve: 'P-256' };
const privateKey = privateKeyObject.toCryptoKey(algorithm, true, ['sign']);
const publicKey = crypto.createPublicKey(privateKeyObject).toCryptoKey(algorithm, true, ['verify']);
const secretKey = crypto.createSecretKey(Buffer.alloc(16)).toCryptoKey('AES-CBC', true, ['encrypt']);

for (const key of [privateKey, publicKey, secretKey]) {
  for (const input of [key, { key, format: 'raw-public', asymmetricKeyType: 'invalid' }]) {
    for (const context of [kConsumePublic, kConsumePrivate, kCreatePublic]) {
      if (key === privateKey || (key === publicKey && context === kConsumePublic)) {
        assert.strictEqual(prepareAsymmetricKey(input, context).data, getCryptoKeyHandle(key));
      } else {
        const expected = context === kConsumePublic ? 'private or public' : 'private';
        assert.throws(() => prepareAsymmetricKey(input, context), {
          code: 'ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE',
          name: 'TypeError',
          message: `Invalid key object type ${key.type}, expected ${expected}.`,
        });
      }
    }
    assert.throws(() => prepareAsymmetricKey(input, kCreatePrivate), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
  }
}
