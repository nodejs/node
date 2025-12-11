'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

const assert = require('assert');
const {
  createPublicKey,
  createPrivateKey,
} = require('crypto');

const fixtures = require('../common/fixtures');

function getKeyFileName(type, suffix) {
  return `${type.replaceAll('-', '_')}_${suffix}.pem`;
}

for (const asymmetricKeyType of ['ml-kem-512', 'ml-kem-768', 'ml-kem-1024']) {
  const keys = {
    public: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'public'), 'ascii'),
    private: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private'), 'ascii'),
    private_seed_only: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private_seed_only'), 'ascii'),
    private_priv_only: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private_priv_only'), 'ascii'),
  };

  function assertKey(key) {
    assert.deepStrictEqual(key.asymmetricKeyDetails, {});
    assert.strictEqual(key.asymmetricKeyType, asymmetricKeyType);
    assert.strictEqual(key.equals(key), true);
    assert.deepStrictEqual(key, key);
  }

  function assertPublicKey(key) {
    assertKey(key);
    assert.strictEqual(key.type, 'public');
    assert.strictEqual(key.export({ format: 'pem', type: 'spki' }), keys.public);
    key.export({ format: 'der', type: 'spki' });
    assert.throws(() => key.export({ format: 'jwk' }),
                  { code: 'ERR_CRYPTO_JWK_UNSUPPORTED_KEY_TYPE', message: 'Unsupported JWK Key Type.' });
  }

  function assertPrivateKey(key, hasSeed) {
    assertKey(key);
    assert.strictEqual(key.type, 'private');
    assertPublicKey(createPublicKey(key));
    key.export({ format: 'der', type: 'pkcs8' });
    if (hasSeed) {
      assert.strictEqual(key.export({ format: 'pem', type: 'pkcs8' }), keys.private);
    } else {
      assert.strictEqual(key.export({ format: 'pem', type: 'pkcs8' }), keys.private_priv_only);
    }
    assert.throws(() => key.export({ format: 'jwk' }),
                  { code: 'ERR_CRYPTO_JWK_UNSUPPORTED_KEY_TYPE', message: 'Unsupported JWK Key Type.' });
  }

  if (!hasOpenSSL(3, 5)) {
    assert.throws(() => createPublicKey(keys.public), {
      code: hasOpenSSL(3) ? 'ERR_OSSL_EVP_DECODE_ERROR' : 'ERR_OSSL_EVP_UNSUPPORTED_ALGORITHM',
    });

    for (const pem of [keys.private, keys.private_seed_only, keys.private_priv_only]) {
      assert.throws(() => createPrivateKey(pem), {
        code: hasOpenSSL(3) ? 'ERR_OSSL_UNSUPPORTED' : 'ERR_OSSL_EVP_UNSUPPORTED_ALGORITHM',
      });
    }
  } else {
    const publicKey = createPublicKey(keys.public);
    assertPublicKey(publicKey);

    {
      for (const [pem, hasSeed] of [
        [keys.private, true],
        [keys.private_seed_only, true],
        [keys.private_priv_only, false],
      ]) {
        const pubFromPriv = createPublicKey(pem);
        assertPublicKey(pubFromPriv);
        assertPrivateKey(createPrivateKey(pem), hasSeed);
        assert.strictEqual(pubFromPriv.equals(publicKey), true);
      }
    }
  }
}
