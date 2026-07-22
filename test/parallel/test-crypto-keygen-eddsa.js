'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// Test EdDSA key generation.
{
  for (const keyType of ['ed25519', 'ed448', 'x25519', 'x448']) {
    if (process.features.openssl_is_boringssl && keyType.endsWith('448')) {
      common.printSkipMessage(`Skipping unsupported ${keyType} test case`);
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
