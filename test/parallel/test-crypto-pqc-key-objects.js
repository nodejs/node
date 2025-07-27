'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 5))
  common.skip('requires OpenSSL 3.5');

const assert = require('assert');
const {
  createPublicKey,
  createPrivateKey,
} = require('crypto');

const fixtures = require('../common/fixtures');

function getKeyFileName(type, suffix) {
  return `${type.replaceAll('-', '_')}_${suffix}.pem`;
}

// ML-DSA
for (const [asymmetricKeyType, pubLen] of [
  ['ml-dsa-44', 1312], ['ml-dsa-65', 1952], ['ml-dsa-87', 2592],
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
    assert.strictEqual(Buffer.from(jwk.priv, 'base64url').byteLength, 32);
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
    assertPublicJwk(key.export({ format: 'jwk' }));
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
      assertPrivateJwk(key.export({ format: 'jwk' }));
    } else {
      assert.throws(() => key.export({ format: 'jwk' }),
                    { code: 'ERR_CRYPTO_OPERATION_FAILED', message: 'key does not have an available seed' });
    }
  }

  const publicKey = createPublicKey(keys.public);
  assertPublicKey(publicKey);
  // TODO(@panva): test public JWK import

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
      assert.deepStrictEqual(pubFromPriv, publicKey);

      if (hasSeed) {
        // TODO(@panva): test private JWK import
        // TODO(@panva): assert private JWK import with mismatched "pub" fails
      } else {
        // TODO(@panva): assert JWK import with "priv" set to the expanded private key form fails
      }
    }
  }
}
