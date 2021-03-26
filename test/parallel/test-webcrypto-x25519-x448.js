'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const {
  generateKeyPairSync,
  webcrypto: { subtle }
} = require('crypto');

// X25519 and X448 are ECDH named curves that should work
// with the existing ECDH mechanisms with no additional
// changes.

async function generateKeys(namedCurve, ...usages) {
  return subtle.generateKey(
    {
      name: 'ECDH',
      namedCurve
    },
    true,
    usages);
}

async function deriveKey(publicKey, privateKey, length = 256) {
  return subtle.deriveKey(
    {
      name: 'ECDH',
      public: publicKey,
    },
    privateKey,
    {
      name: 'HMAC',
      length,
      hash: 'SHA-512',
    },
    true,
    ['sign', 'verify']
  );
}

async function exportKey(secretKey) {
  return subtle.exportKey('raw', secretKey);
}

async function importKey(namedCurve, keyData, isPublic = false) {
  return subtle.importKey(
    'raw',
    keyData,
    { name: 'ECDH', namedCurve, public: isPublic },
    true,
    ['deriveKey']
  );
}

assert.rejects(importKey('NODE-X25519', Buffer.alloc(10), true), {
  message: /NODE-X25519 raw keys must be exactly 32-bytes/
}).then(common.mustCall());
assert.rejects(importKey('NODE-X448', Buffer.alloc(10), true), {
  message: /NODE-X448 raw keys must be exactly 56-bytes/
}).then(common.mustCall());

async function test1(namedCurve) {
  const {
    publicKey: publicKey1,
    privateKey: privateKey1,
  } = await generateKeys(namedCurve, 'deriveKey', 'deriveBits');

  const {
    publicKey: publicKey2,
    privateKey: privateKey2,
  } = await generateKeys(namedCurve, 'deriveKey', 'deriveBits');

  assert(publicKey1);
  assert(privateKey1);
  assert(publicKey2);
  assert(privateKey2);

  assert.strictEqual(publicKey1.algorithm.namedCurve, namedCurve);
  assert.strictEqual(privateKey1.algorithm.namedCurve, namedCurve);
  assert.strictEqual(publicKey2.algorithm.namedCurve, namedCurve);
  assert.strictEqual(privateKey2.algorithm.namedCurve, namedCurve);

  const [key1, key2] = await Promise.all([
    deriveKey(publicKey1, privateKey2),
    deriveKey(publicKey2, privateKey1),
  ]);

  assert(key1);
  assert(key2);

  const [secret1, secret2] = await Promise.all([
    exportKey(key1),
    exportKey(key2),
  ]);

  assert.deepStrictEqual(secret1, secret2);
}

Promise.all([
  test1('NODE-X25519'),
  test1('NODE-X448'),
]).then(common.mustCall());

const testVectors = {
  'NODE-X25519': {
    alice: {
      privateKey:
        Buffer.from(
          '77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a',
          'hex'),
      publicKey:
        Buffer.from(
          '8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a',
          'hex'),
    },
    bob: {
      privateKey:
        Buffer.from(
          '5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb',
          'hex'),
      publicKey:
        Buffer.from(
          'de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f',
          'hex'),
    },
    sharedSecret:
      Buffer.from(
        '4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742',
        'hex'),
  },
  'NODE-X448': {
    alice: {
      privateKey:
        Buffer.from(
          '9a8f4925d1519f5775cf46b04b5800d4ee9ee8bae8bc5565d498c28d' +
          'd9c9baf574a9419744897391006382a6f127ab1d9ac2d8c0a598726b',
          'hex'),
      publicKey:
        Buffer.from(
          '9b08f7cc31b7e3e67d22d5aea121074a273bd2b83de09c63faa73d2c' +
          '22c5d9bbc836647241d953d40c5b12da88120d53177f80e532c41fa0',
          'hex'),
    },
    bob: {
      privateKey:
        Buffer.from(
          '1c306a7ac2a0e2e0990b294470cba339e6453772b075811d8fad0d1d' +
          '6927c120bb5ee8972b0d3e21374c9c921b09d1b0366f10b65173992d',
          'hex'),
      publicKey:
        Buffer.from(
          '3eb7a829b0cd20f5bcfc0b599b6feccf6da4627107bdb0d4f345b430' +
          '27d8b972fc3e34fb4232a13ca706dcb57aec3dae07bdc1c67bf33609',
          'hex'),
    },
    sharedSecret:
      Buffer.from(
        '07fff4181ac6cc95ec1c16a94a0f74d12da232ce40a77552281d282b' +
        'b60c0b56fd2464c335543936521c24403085d59a449a5037514a879d',
        'hex'),
  },
};

async function test2(namedCurve, length) {
  const [
    publicKey1,
    publicKey2,
    privateKey1,
    privateKey2,
  ] = await Promise.all([
    importKey(namedCurve, testVectors[namedCurve].alice.publicKey, true),
    importKey(namedCurve, testVectors[namedCurve].bob.publicKey, true),
    importKey(namedCurve, testVectors[namedCurve].alice.privateKey),
    importKey(namedCurve, testVectors[namedCurve].bob.privateKey),
  ]);

  const [key1, key2] = await Promise.all([
    deriveKey(publicKey1, privateKey2, length),
    deriveKey(publicKey2, privateKey1, length),
  ]);

  assert(key1);
  assert(key2);

  const [secret1, secret2] = await Promise.all([
    exportKey(key1),
    exportKey(key2),
  ]);

  assert.deepStrictEqual(secret1, secret2);

  assert.deepStrictEqual(
    Buffer.from(secret1),
    testVectors[namedCurve].sharedSecret);

  const [
    publicKeyJwk,
    privateKeyJwk,
  ] = await Promise.all([
    subtle.exportKey('jwk', publicKey1),
    subtle.exportKey('jwk', privateKey1),
  ]);
  assert.strictEqual(publicKeyJwk.kty, 'OKP');
  assert.strictEqual(privateKeyJwk.kty, 'OKP');
  assert.strictEqual(publicKeyJwk.crv, namedCurve.slice(5));
  assert.strictEqual(privateKeyJwk.crv, namedCurve.slice(5));
  assert.deepStrictEqual(
    Buffer.from(publicKeyJwk.x, 'base64'),
    testVectors[namedCurve].alice.publicKey);
  assert.deepStrictEqual(
    Buffer.from(privateKeyJwk.x, 'base64'),
    testVectors[namedCurve].alice.publicKey);
  assert.deepStrictEqual(
    Buffer.from(privateKeyJwk.d, 'base64'),
    testVectors[namedCurve].alice.privateKey);
}

Promise.all([
  test2('NODE-X25519', 256),
  test2('NODE-X448', 448),
]).then(common.mustCall());

assert.rejects(
  subtle.generateKey(
    {
      name: 'ECDH',
      namedCurve: 'NODE-ED25519'
    },
    true,
    ['deriveBits']),
  {
    message: /Unsupported named curves for ECDH/
  }).then(common.mustCall());

assert.rejects(
  subtle.generateKey(
    {
      name: 'ECDH',
      namedCurve: 'NODE-ED448'
    },
    true,
    ['deriveBits']),
  {
    message: /Unsupported named curves for ECDH/
  }).then(common.mustCall());

{
  // Private JWK import
  subtle.importKey(
    'jwk',
    {
      crv: 'X25519',
      d: '8CE-XY7cvbR-Pu7mILHq8YZ4hLGAA2-RD01he5q2wUA',
      x: '42IbTo34ZYANub5o42547vB6OxdEd44ztwZewoRch0Q',
      kty: 'OKP'
    },
    {
      name: 'ECDH',
      namedCurve: 'NODE-X25519'
    },
    true,
    ['deriveBits']).then(common.mustCall(), common.mustNotCall());

  // Public JWK import
  subtle.importKey(
    'jwk',
    {
      crv: 'X25519',
      x: '42IbTo34ZYANub5o42547vB6OxdEd44ztwZewoRch0Q',
      kty: 'OKP'
    },
    {
      name: 'ECDH',
      namedCurve: 'NODE-X25519'
    },
    true,
    []).then(common.mustCall(), common.mustNotCall());

  for (const asymmetricKeyType of ['x25519', 'x448']) {
    const { publicKey, privateKey } = generateKeyPairSync(asymmetricKeyType);
    for (const keyObject of [publicKey, privateKey]) {
      const namedCurve = `NODE-${asymmetricKeyType.toUpperCase()}`;
      subtle.importKey(
        'node.keyObject',
        keyObject,
        { name: 'ECDH', namedCurve },
        true,
        keyObject.type === 'private' ? ['deriveBits', 'deriveKey'] : [],
      ).then((cryptoKey) => {
        assert.strictEqual(cryptoKey.type, keyObject.type);
        assert.strictEqual(cryptoKey.algorithm.name, 'ECDH');
      }, common.mustNotCall());
    }
  }
}
