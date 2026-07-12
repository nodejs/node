'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');
const { hasFIPS } = require('../common/crypto');
const rejectsXCurves = hasFIPS(3, 5);

// Test EdDSA key generation.
{
  for (const keyType of ['ed25519', 'ed448', 'x25519', 'x448']) {
    if (process.features.openssl_is_boringssl && keyType.endsWith('448')) {
      common.printSkipMessage(`Skipping unsupported ${keyType} test case`);
      continue;
    }
    generateKeyPair(keyType, common.mustCall((err, publicKey, privateKey) => {
      if (rejectsXCurves && keyType.startsWith('x')) {
        assert.strictEqual(err?.code, 'ERR_OSSL_EVP_UNSUPPORTED');
        return;
      }
      assert.ifError(err);
      assert.strictEqual(publicKey.type, 'public');
      assert.strictEqual(publicKey.asymmetricKeyType, keyType);
      assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {});

      assert.strictEqual(privateKey.type, 'private');
      assert.strictEqual(privateKey.asymmetricKeyType, keyType);
      assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {});
    }));
  }
}
