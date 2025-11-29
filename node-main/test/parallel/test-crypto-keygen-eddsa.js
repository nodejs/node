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
  if (!/^1\.1\.0/.test(process.versions.openssl)) {
    ['ed25519', 'ed448', 'x25519', 'x448'].forEach((keyType) => {
      generateKeyPair(keyType, common.mustSucceed((publicKey, privateKey) => {
        assert.strictEqual(publicKey.type, 'public');
        assert.strictEqual(publicKey.asymmetricKeyType, keyType);
        assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {});

        assert.strictEqual(privateKey.type, 'private');
        assert.strictEqual(privateKey.asymmetricKeyType, keyType);
        assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {});
      }));
    });
  }
}
