'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

const algorithms = process.features.openssl_is_boringssl ?
  // BoringSSL does not support ML-KEM-512.
  ['ml-kem-768', 'ml-kem-1024'] :
  ['ml-kem-512', 'ml-kem-768', 'ml-kem-1024'];

if (!hasOpenSSL(3, 5) && !process.features.openssl_is_boringssl) {
  for (const asymmetricKeyType of ['ml-kem-512', 'ml-kem-768', 'ml-kem-1024']) {
    assert.throws(() => generateKeyPair(asymmetricKeyType, common.mustNotCall()), {
      code: 'ERR_INVALID_ARG_VALUE',
      message: /The argument 'type' must be a supported key type/
    });
  }
} else {
  for (const asymmetricKeyType of algorithms) {
    function assertJwk(jwk) {
      assert.strictEqual(jwk.kty, 'AKP');
      assert.strictEqual(jwk.alg, asymmetricKeyType.toUpperCase());
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

    for (const [publicKeyEncoding, validate] of [
      /* eslint-disable node-core/must-call-assert */
      [undefined, (publicKey) => {
        assert.strictEqual(publicKey.type, 'public');
        assert.strictEqual(publicKey.asymmetricKeyType, asymmetricKeyType);
        assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {});
      }],
      [{ format: 'jwk' }, (publicKey) => assertPublicJwk(publicKey)],
      [{ format: 'pem', type: 'spki' }, (publicKey) => assert.strictEqual(typeof publicKey, 'string')],
      [{ format: 'der', type: 'spki' }, (publicKey) => assert.strictEqual(Buffer.isBuffer(publicKey), true)],
      /* eslint-enable node-core/must-call-assert */
    ]) {
      generateKeyPair(asymmetricKeyType, { publicKeyEncoding }, common.mustSucceed(validate));
    }
    for (const [privateKeyEncoding, validate] of [
      /* eslint-disable node-core/must-call-assert */
      [undefined, (_, privateKey) => {
        assert.strictEqual(privateKey.type, 'private');
        assert.strictEqual(privateKey.asymmetricKeyType, asymmetricKeyType);
        assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {});
      }],
      [{ format: 'jwk' }, (_, privateKey) => assertPrivateJwk(privateKey)],
      [{ format: 'pem', type: 'pkcs8' }, (_, privateKey) => assert.strictEqual(typeof privateKey, 'string')],
      [{ format: 'der', type: 'pkcs8' }, (_, privateKey) => assert.strictEqual(Buffer.isBuffer(privateKey), true)],
      /* eslint-enable node-core/must-call-assert */
    ]) {
      generateKeyPair(asymmetricKeyType, { privateKeyEncoding }, common.mustSucceed(validate));
    }
  }
}

if (process.features.openssl_is_boringssl) {
  assert.throws(() => generateKeyPair('ml-kem-512', common.mustNotCall()), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /The argument 'type' must be a supported key type/
  });
}
