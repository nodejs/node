'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 5))
  common.skip('requires OpenSSL 3.5');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// ML-DSA
{
  for (const [asymmetricKeyType, pubLen] of [
    ['ml-dsa-44', 1312], ['ml-dsa-65', 1952], ['ml-dsa-87', 2592],
  ]) {

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

    for (const [publicKeyEncoding, validate] of [
      [undefined, (publicKey) => {
        assert.strictEqual(publicKey.type, 'public');
        assert.strictEqual(publicKey.asymmetricKeyType, asymmetricKeyType);
        assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {});
      }],
      [{ format: 'jwk' }, (publicKey) => assertPublicJwk(publicKey)],
      [{ format: 'pem', type: 'spki' }, (publicKey) => assert.strictEqual(typeof publicKey, 'string')],
      [{ format: 'der', type: 'spki' }, (publicKey) => assert.strictEqual(Buffer.isBuffer(publicKey), true)],
    ]) {
      generateKeyPair(asymmetricKeyType, { publicKeyEncoding }, common.mustSucceed(validate));
    }
    for (const [privateKeyEncoding, validate] of [
      [undefined, (_, privateKey) => {
        assert.strictEqual(privateKey.type, 'private');
        assert.strictEqual(privateKey.asymmetricKeyType, asymmetricKeyType);
        assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {});
      }],
      [{ format: 'jwk' }, (_, privateKey) => assertPrivateJwk(privateKey)],
      [{ format: 'pem', type: 'pkcs8' }, (_, privateKey) => assert.strictEqual(typeof privateKey, 'string')],
      [{ format: 'der', type: 'pkcs8' }, (_, privateKey) => assert.strictEqual(Buffer.isBuffer(privateKey), true)],
    ]) {
      generateKeyPair(asymmetricKeyType, { privateKeyEncoding }, common.mustSucceed(validate));
    }
  }
}
