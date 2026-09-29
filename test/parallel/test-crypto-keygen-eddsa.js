'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
  generateKeyPairSync,
} = require('crypto');
const { hasFIPS, isBoringSSL } = require('../common/crypto');
const rejectsXCurves = hasFIPS(3, 5);

// Named key generation accepts only supported, lowercase public key types.
for (const type of [
  'toString', 'constructor', 'sm2', 'ml-kem-999',
  'Ed25519', 'X25519', 'ML-KEM-768', 'ML-DSA-44', 'SLH-DSA-SHA2-128f',
]) {
  const error = {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: `The argument 'type' must be a supported key type. Received '${type}'`,
  };
  assert.throws(() => generateKeyPairSync(type), error);
  assert.throws(() => generateKeyPair(type, common.mustNotCall()), error);
}

// Test EdDSA key generation.
{
  for (const keyType of ['ed25519', 'ed448', 'x25519', 'x448']) {
    if ((isBoringSSL && keyType.endsWith('448')) ||
        (rejectsXCurves && keyType.startsWith('x'))) {
      const error = {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_VALUE',
        message: `The argument 'type' must be a supported key type. Received '${keyType}'`,
      };
      assert.throws(() => generateKeyPairSync(keyType), error);
      assert.throws(() => generateKeyPair(keyType, common.mustNotCall()), error);
      continue;
    }
    generateKeyPair(keyType, common.mustSucceed((publicKey, privateKey) => {
      assert.strictEqual(publicKey.type, 'public');
      assert.strictEqual(publicKey.asymmetricKeyType, keyType);
      assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {});

      assert.strictEqual(privateKey.type, 'private');
      assert.strictEqual(privateKey.asymmetricKeyType, keyType);
      assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {});
    }));
  }
}
