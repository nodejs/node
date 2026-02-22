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
  ['slh-dsa-sha2-128f', 32], ['slh-dsa-sha2-128s', 32], ['slh-dsa-sha2-192f', 48], ['slh-dsa-sha2-192s', 48],
  ['slh-dsa-sha2-256f', 64], ['slh-dsa-sha2-256s', 64], ['slh-dsa-shake-128f', 32], ['slh-dsa-shake-128s', 32],
  ['slh-dsa-shake-192f', 48], ['slh-dsa-shake-192s', 48], ['slh-dsa-shake-256f', 64], ['slh-dsa-shake-256s', 64],
]) {
  const keys = {
    public: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'public'), 'ascii'),
    private: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private'), 'ascii'),
  };

  function assertJwk(jwk) {
    assert.strictEqual(jwk.kty, 'AKP');
    assert.strictEqual(jwk.alg, asymmetricKeyType.toUpperCase().slice(0, -1) + asymmetricKeyType.slice(-1));
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
    assert.strictEqual(Buffer.from(jwk.priv, 'base64url').byteLength, pubLen * 2);
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
