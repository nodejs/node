'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

if (!hasOpenSSL(3, 5)) {
  for (const asymmetricKeyType of ['ml-kem-512', 'ml-kem-768', 'ml-kem-1024']) {
    assert.throws(() => generateKeyPair(asymmetricKeyType, common.mustNotCall()), {
      code: 'ERR_INVALID_ARG_VALUE',
      message: /The argument 'type' must be a supported key type/
    });
  }
} else {
  for (const asymmetricKeyType of ['ml-kem-512', 'ml-kem-768', 'ml-kem-1024']) {
    for (const [publicKeyEncoding, validate] of [
      [undefined, (publicKey) => {
        assert.strictEqual(publicKey.type, 'public');
        assert.strictEqual(publicKey.asymmetricKeyType, asymmetricKeyType);
        assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {});
      }],
      [{ format: 'pem', type: 'spki' }, (publicKey) => assert.strictEqual(typeof publicKey, 'string')],
      [{ format: 'der', type: 'spki' }, (publicKey) => assert.strictEqual(Buffer.isBuffer(publicKey), true)],
    ]) {
      generateKeyPair(asymmetricKeyType, { publicKeyEncoding }, common.mustSucceed(validate));
    }
    for (const [privateKeyEncoding, validate] of [
      [undefined, (_, privateKey) => {
        assert.strictEqual(privateKey.type, 'private');
        assert.strictEqual(privateKey.asymmetricKeyType, asymmetricKeyType);
        assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {});
      }],
      [{ format: 'pem', type: 'pkcs8' }, (_, privateKey) => assert.strictEqual(typeof privateKey, 'string')],
      [{ format: 'der', type: 'pkcs8' }, (_, privateKey) => assert.strictEqual(Buffer.isBuffer(privateKey), true)],
    ]) {
      generateKeyPair(asymmetricKeyType, { privateKeyEncoding }, common.mustSucceed(validate));
    }
  }
}
