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

for (const asymmetricKeyType of [
  'slh-dsa-sha2-128f', 'slh-dsa-sha2-128s', 'slh-dsa-sha2-192f', 'slh-dsa-sha2-192s',
  'slh-dsa-sha2-256f', 'slh-dsa-sha2-256s', 'slh-dsa-shake-128f', 'slh-dsa-shake-128s',
  'slh-dsa-shake-192f', 'slh-dsa-shake-192s', 'slh-dsa-shake-256f', 'slh-dsa-shake-256s',
]) {
  const keys = {
    public: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'public'), 'ascii'),
    private: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private'), 'ascii'),
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

  function assertPrivateKey(key) {
    assertKey(key);
    assert.strictEqual(key.type, 'private');
    assertPublicKey(createPublicKey(key));
    key.export({ format: 'der', type: 'pkcs8' });
    assert.strictEqual(key.export({ format: 'pem', type: 'pkcs8' }), keys.private);
    assert.throws(() => key.export({ format: 'jwk' }),
                  { code: 'ERR_CRYPTO_JWK_UNSUPPORTED_KEY_TYPE', message: 'Unsupported JWK Key Type.' });
  }

  if (!hasOpenSSL(3, 5)) {
    assert.throws(() => createPublicKey(keys.public), {
      code: hasOpenSSL(3) ? 'ERR_OSSL_EVP_DECODE_ERROR' : 'ERR_OSSL_EVP_UNSUPPORTED_ALGORITHM',
    });

    assert.throws(() => createPrivateKey(keys.private), {
      code: hasOpenSSL(3) ? 'ERR_OSSL_UNSUPPORTED' : 'ERR_OSSL_EVP_UNSUPPORTED_ALGORITHM',
    });
  } else {
    const publicKey = createPublicKey(keys.public);
    assertPublicKey(publicKey);

    const pubFromPriv = createPublicKey(keys.private);
    assertPublicKey(pubFromPriv);
    assertPrivateKey(createPrivateKey(keys.private));
    assert.strictEqual(pubFromPriv.equals(publicKey), true);
  }
}
