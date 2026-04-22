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
    jwk: JSON.parse(fixtures.readKey(`${asymmetricKeyType}.json`)),
  };

  function assertJwk(jwk) {
    assert.strictEqual(jwk.kty, 'AKP');
    // SLH-DSA algorithm names keep the last character (f/s) lowercase.
    const expectedAlg = asymmetricKeyType.slice(0, -1).toUpperCase() +
                        asymmetricKeyType.slice(-1);
    assert.strictEqual(jwk.alg, expectedAlg);
    assert.ok(jwk.pub);
  }

  function assertPublicJwk(jwk) {
    assertJwk(jwk);
    assert.ok(!jwk.priv);
  }

  function assertPrivateJwk(jwk) {
    assertJwk(jwk);
    assert.ok(jwk.priv);
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
    const importedPub = createPublicKey({
      key: rawPub, format: 'raw-public', asymmetricKeyType,
    });
    assert.strictEqual(importedPub.equals(key), true);
  }

  function assertPrivateKey(key) {
    assertKey(key);
    assert.strictEqual(key.type, 'private');
    assertPublicKey(createPublicKey(key));
    key.export({ format: 'der', type: 'pkcs8' });
    assert.strictEqual(key.export({ format: 'pem', type: 'pkcs8' }), keys.private);
    const jwk = key.export({ format: 'jwk' });
    assertPrivateJwk(jwk);
    assert.strictEqual(key.equals(createPrivateKey({ format: 'jwk', key: jwk })), true);
    assert.ok(createPublicKey({ format: 'jwk', key: jwk }));

    // Raw format round-trip
    const rawPriv = key.export({ format: 'raw-private' });
    assert(Buffer.isBuffer(rawPriv));
    const importedPriv = createPrivateKey({
      key: rawPriv, format: 'raw-private', asymmetricKeyType,
    });
    assert.strictEqual(importedPriv.equals(key), true);
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
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, priv: Buffer.alloc(1).toString('base64url') } }),
                  { code: 'ERR_CRYPTO_INVALID_JWK' });
    // eslint-disable-next-line @stylistic/js/max-len
    assert.throws(() => createPublicKey({ format, key: { kty: jwk.kty, alg: jwk.alg, pub: Buffer.alloc(1).toString('base64url') } }),
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
