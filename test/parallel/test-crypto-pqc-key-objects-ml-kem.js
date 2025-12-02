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

for (const [asymmetricKeyType, pubLen] of [
  ['ml-kem-512', 800], ['ml-kem-768', 1184], ['ml-kem-1024', 1568],
]) {
  const keys = {
    public: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'public'), 'ascii'),
    private: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private'), 'ascii'),
    private_seed_only: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private_seed_only'), 'ascii'),
    private_priv_only: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private_priv_only'), 'ascii'),
  };

  function assertJwk(jwk) {
    assert.strictEqual(jwk.kty, 'AKP');
    assert.strictEqual(jwk.alg, asymmetricKeyType.toUpperCase());
    assert.ok(jwk.pub);
    assert.strictEqual(Buffer.from(jwk.pub, 'base64url').byteLength, pubLen);
  }

  function assertPublicJwk(jwk) {
    assertJwk(jwk);
    assert.ok(!jwk.priv);
  }

  function assertPrivateJwk(jwk) {
    assertJwk(jwk);
    assert.ok(jwk.priv);
    assert.strictEqual(Buffer.from(jwk.priv, 'base64url').byteLength, 64);
  }

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
    const jwk = key.export({ format: 'jwk' });
    assertPublicJwk(jwk);
    assert.strictEqual(key.equals(createPublicKey({ format: 'jwk', key: jwk })), true);
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
    if (hasSeed) {
      const jwk = key.export({ format: 'jwk' });
      assertPrivateJwk(jwk);
      assert.strictEqual(key.equals(createPrivateKey({ format: 'jwk', key: jwk })), true);
      assert.ok(createPublicKey({ format: 'jwk', key: jwk }));
    } else {
      assert.throws(() => key.export({ format: 'jwk' }),
                    { code: 'ERR_CRYPTO_OPERATION_FAILED', message: 'key does not have an available seed' });
    }
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

{
  const format = 'jwk';
  const jwk = {
    kty: 'AKP',
    alg: 'ML-KEM-512',
    priv: 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA',
    // eslint-disable-next-line @stylistic/js/max-len
    pub: '3xeEhndBbpVNZvmwnhKBUyoujwxqvgA35-gRkJfJ7IRaoGmFwIhVLEG2FRc2QsECUcBukaJcJDwCY7Z1xyB9WHAdE6Uuq5JWXvGh3vuu_kwLA_UERKO-ILFxMQuj8ghS86KhjnIpQHAVZGBS0uc6vuMY11WJnHhKb7WqsXyQLLaK-jZXuQEuG7i2qsJomqSMLUJiuyljsGokIoJUHPYUGdkOK9NmvuekNptyQaZr8mRdjpBMP3swDAQdVoc5PSypKXXL_cAY3cYK9WIAnrCIjloFdiRqznRbsWNAi1qa48GUqFwhkHs3sWKOzZoV7CAkhzAnNESwovRU-bRzDzORUEdu4HCY9rwbl8wb07jBousOUKeC8hFd9hdJb2xvjAmwX4iNnpM9KHdGxjGzEIcmsMSnyIsJyGCs11I1cMgCvThyQxYtEoyiKUyDQxjMIfsU0qs3PyJOP9SYQ4WVCfTKGhpWbAVniKOqSEqqvfGgjxtEx1arKgyLxYUe4usjA0wuqlrBX4l1G9XK4dWQidUAK7Gzv8pK3gmCrWeD6dN_s-MgrJhV32Zr_Xxrh6mkbiWXElYRYVB-FzWY-YistvmrOhCSYyRGiMcIeHWP9NQxO3Zk8PUQ6RPKAcUrOhtGU1FE4tsMrOimRmYAzyqHg1AH4WqlB4Adhrc4Xmab-va6HfVoMWPC1XBTL9dDkFRkDyRMUocTWdKEL8M3oGADOvBeqgAcNPx72PkQKeRsKUM2J2RdZ4ZkIdZhryV0gFMriHhQ3Emf_tWxQJigM3Jeggpb4UAKDLcDdBynS0eGc6rPhRZu6KGE2yxYVJwiQLgwjCe-ukDDudcp7bGMwgaOzbJ9oiss0-eguqUwxRk808hr-GpE8HmeUVWwm-JQmIUjs6cx3YkbwgBgEaBlwKtX8abBeIlV4BNfpcqPPlLHXTcWlzovsiwOsXwbMoUp2fp2VtdN6nQNnweXd8NsF6GcGViehNK45NG8MQfL0ngUIkg1SERsiZMUd0SqnsHFk-wtW6rIagr2SoXpCd-OKBZgXSC044KzC7thvzpfghoLXbqa0-c',
  };

  if (hasOpenSSL(3, 5)) {
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, alg: 'ml-kem-512' } }),
                  { code: 'ERR_INVALID_ARG_VALUE', message: /must be one of: 'ML-DSA-44', 'ML-DSA-65', 'ML-DSA-87', 'ML-KEM-512', 'ML-KEM-768', 'ML-KEM-1024'/ });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, alg: undefined } }),
                  { code: 'ERR_INVALID_ARG_VALUE', message: /must be one of: 'ML-DSA-44', 'ML-DSA-65', 'ML-DSA-87', 'ML-KEM-512', 'ML-KEM-768', 'ML-KEM-1024'/ });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, pub: undefined } }),
                  { code: 'ERR_INVALID_ARG_TYPE', message: /The "key\.pub" property must be of type string/ });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, priv: undefined } }),
                  { code: 'ERR_INVALID_ARG_TYPE', message: /The "key\.priv" property must be of type string/ });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, priv: Buffer.alloc(65).toString('base64url') } }),
                  { code: 'ERR_CRYPTO_INVALID_JWK' });
    assert.throws(() => createPublicKey({ format, key: { ...jwk, pub: Buffer.alloc(801).toString('base64url') } }),
                  { code: 'ERR_CRYPTO_INVALID_JWK' });

    assert.ok(createPrivateKey({ format, key: jwk }));
    assert.ok(createPublicKey({ format, key: jwk }));
  } else {
    assert.throws(() => createPrivateKey({ format, key: jwk }),
                  { code: 'ERR_INVALID_ARG_VALUE', message: /must be one of: 'RSA', 'EC', 'OKP'\. Received 'AKP'/ });
    assert.throws(() => createPublicKey({ format, key: jwk }),
                  { code: 'ERR_INVALID_ARG_VALUE', message: /must be one of: 'RSA', 'EC', 'OKP'\. Received 'AKP'/ });
  }
}
