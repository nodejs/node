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
    jwk: JSON.parse(fixtures.readKey(`${asymmetricKeyType}.json`)),
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

    // Raw format round-trip
    const rawPub = key.export({ format: 'raw-public' });
    assert(Buffer.isBuffer(rawPub));
    assert.strictEqual(rawPub.byteLength, pubLen);
    const importedPub = createPublicKey({
      key: rawPub, format: 'raw-public', asymmetricKeyType,
    });
    assert.strictEqual(importedPub.equals(key), true);
  }

  function assertPrivateKey(key, hasSeed) {
    assertKey(key);
    assert.strictEqual(key.type, 'private');
    assertPublicKey(createPublicKey(key));
    key.export({ format: 'der', type: 'pkcs8' });
    if (hasSeed) {
      assert.strictEqual(key.export({ format: 'pem', type: 'pkcs8' }), keys.private_seed_only);
      const jwk = key.export({ format: 'jwk' });
      assertPrivateJwk(jwk);
      assert.strictEqual(key.equals(createPrivateKey({ format: 'jwk', key: jwk })), true);
      assert.ok(createPublicKey({ format: 'jwk', key: jwk }));

      // Raw seed round-trip
      const rawSeed = key.export({ format: 'raw-seed' });
      assert(Buffer.isBuffer(rawSeed));
      assert.strictEqual(rawSeed.byteLength, 64);
      const importedPriv = createPrivateKey({
        key: rawSeed, format: 'raw-seed', asymmetricKeyType,
      });
      assert.strictEqual(importedPriv.equals(key), true);
    } else {
      assert.strictEqual(key.export({ format: 'pem', type: 'pkcs8' }), keys.private_priv_only);
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

    // JWK import error tests
    const format = 'jwk';
    const jwk = keys.jwk;

    assert.throws(() => createPrivateKey({ format, key: { ...jwk, alg: asymmetricKeyType } }),
                  { code: 'ERR_CRYPTO_INVALID_JWK' });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, alg: undefined } }),
                  { code: 'ERR_CRYPTO_INVALID_JWK' });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, pub: undefined } }),
                  { code: 'ERR_CRYPTO_INVALID_JWK' });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, priv: undefined } }),
                  { code: 'ERR_CRYPTO_INVALID_JWK', message: /JWK does not contain private key material/ });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, priv: Buffer.alloc(65).toString('base64url') } }),
                  { code: 'ERR_CRYPTO_INVALID_JWK' });
    // eslint-disable-next-line @stylistic/js/max-len
    assert.throws(() => createPublicKey({ format, key: { kty: jwk.kty, alg: jwk.alg, pub: Buffer.alloc(pubLen + 1).toString('base64url') } }),
                  { code: 'ERR_CRYPTO_INVALID_JWK' });

    // Importing a private JWK where pub does not match priv should fail.
    assert.throws(
      () => createPrivateKey({
        format,
        key: { ...jwk, pub: `${jwk.pub[0] === 'A' ? 'B' : 'A'}${jwk.pub.slice(1)}` },
      }),
      { code: 'ERR_CRYPTO_INVALID_JWK' }
    );

    // JWK round-trip
    assert.partialDeepStrictEqual(jwk, createPublicKey({ format, key: jwk }).export({ format }));
    assert.deepStrictEqual(createPrivateKey({ format, key: jwk }).export({ format }), jwk);
  }
}
