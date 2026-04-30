'use strict';

// Regression test for https://github.com/nodejs/node/issues/62899
// SubtleCrypto.generateKey(), SubtleCrypto.importKey(), and
// KeyObject.prototype.toCryptoKey() should produce CryptoKey values whose
// `usages` have been de-duplicated and returned in a canonical order.
// The same applies to `key_ops` on JWK exports of extractable keys.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createSecretKey } = require('crypto');
const { hasOpenSSL } = require('../common/crypto');
const { subtle } = globalThis.crypto;

function assertSameSet(actual, expected, msg) {
  if (msg === undefined) {
    assert.deepStrictEqual(actual, expected);
  } else {
    assert.deepStrictEqual(actual, expected, msg);
  }
}

{
  const tests = [];

  // Symmetric keys (single CryptoKey result). Inputs are deliberately in
  // non-canonical order so the test exercises the canonical re-ordering.
  const symmetric = [
    { algorithm: { name: 'HMAC', hash: 'SHA-256' },
      usages: ['verify', 'sign', 'verify', 'sign'],
      expected: ['sign', 'verify'] },
    { algorithm: { name: 'AES-CTR', length: 128 },
      usages: ['wrapKey', 'decrypt', 'encrypt', 'unwrapKey', 'wrapKey', 'encrypt'],
      expected: ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'] },
    { algorithm: { name: 'AES-CBC', length: 128 },
      usages: ['encrypt', 'encrypt'],
      expected: ['encrypt'] },
    { algorithm: { name: 'AES-GCM', length: 128 },
      usages: ['decrypt', 'encrypt', 'decrypt'],
      expected: ['encrypt', 'decrypt'] },
  ];

  if (!process.features.openssl_is_boringssl) {
    symmetric.push({
      algorithm: { name: 'AES-KW', length: 128 },
      usages: ['wrapKey', 'unwrapKey', 'wrapKey', 'unwrapKey'],
      expected: ['wrapKey', 'unwrapKey'],
    });
  } else {
    common.printSkipMessage('AES-KW is not supported in BoringSSL');
  }

  if (hasOpenSSL(3)) {
    symmetric.push({
      algorithm: { name: 'AES-OCB', length: 128 },
      usages: ['decrypt', 'encrypt', 'decrypt', 'encrypt'],
      expected: ['encrypt', 'decrypt'],
    });
    symmetric.push({
      algorithm: { name: 'KMAC128', length: 128 },
      usages: ['verify', 'sign', 'verify', 'sign'],
      expected: ['sign', 'verify'],
    });
  } else {
    common.printSkipMessage('AES-OCB and KMAC require OpenSSL >= 3');
  }

  if (!process.features.openssl_is_boringssl) {
    symmetric.push({
      algorithm: { name: 'ChaCha20-Poly1305' },
      usages: ['wrapKey', 'decrypt', 'encrypt', 'unwrapKey', 'wrapKey', 'encrypt'],
      expected: ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'],
    });
  } else {
    common.printSkipMessage('ChaCha20-Poly1305 is not supported in BoringSSL');
  }

  for (const { algorithm, usages, expected } of symmetric) {
    tests.push((async () => {
      const key = await subtle.generateKey(algorithm, true, usages);
      assertSameSet(key.usages, expected,
                    `generateKey ${algorithm.name}`);
      assert.strictEqual(key.usages.length, expected.length,
                         `generateKey ${algorithm.name} usage count`);
    })());
  }

  // Asymmetric keys (CryptoKeyPair result). Duplicates across the input
  // must not produce duplicates on either the public or private key.
  const asymmetric = [
    { algorithm: { name: 'RSA-OAEP', modulusLength: 2048,
                   publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256' },
      usages: ['wrapKey', 'unwrapKey', 'decrypt', 'encrypt',
               'unwrapKey', 'wrapKey', 'decrypt', 'encrypt'],
      publicExpected: ['encrypt', 'wrapKey'],
      privateExpected: ['decrypt', 'unwrapKey'] },
    { algorithm: { name: 'RSA-PSS', modulusLength: 2048,
                   publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256' },
      usages: ['verify', 'sign', 'verify', 'sign'],
      publicExpected: ['verify'],
      privateExpected: ['sign'] },
    { algorithm: { name: 'ECDSA', namedCurve: 'P-256' },
      usages: ['verify', 'sign', 'verify', 'sign', 'verify'],
      publicExpected: ['verify'],
      privateExpected: ['sign'] },
    { algorithm: { name: 'ECDH', namedCurve: 'P-256' },
      usages: ['deriveBits', 'deriveKey', 'deriveBits', 'deriveKey'],
      publicExpected: [],
      privateExpected: ['deriveKey', 'deriveBits'] },
    { algorithm: { name: 'Ed25519' },
      usages: ['verify', 'sign', 'verify', 'sign'],
      publicExpected: ['verify'],
      privateExpected: ['sign'] },
    { algorithm: { name: 'X25519' },
      usages: ['deriveBits', 'deriveKey', 'deriveBits', 'deriveKey'],
      publicExpected: [],
      privateExpected: ['deriveKey', 'deriveBits'] },
  ];

  if (hasOpenSSL(3, 5)) {
    asymmetric.push({
      algorithm: { name: 'ML-DSA-65' },
      usages: ['verify', 'sign', 'verify', 'sign'],
      publicExpected: ['verify'],
      privateExpected: ['sign'],
    });
    asymmetric.push({
      algorithm: { name: 'ML-KEM-768' },
      usages: ['decapsulateBits', 'encapsulateBits', 'decapsulateKey',
               'encapsulateKey', 'decapsulateBits', 'encapsulateBits'],
      publicExpected: ['encapsulateKey', 'encapsulateBits'],
      privateExpected: ['decapsulateKey', 'decapsulateBits'],
    });
  } else {
    common.printSkipMessage('ML-DSA and ML-KEM require OpenSSL >= 3.5');
  }

  for (const { algorithm, usages, publicExpected, privateExpected } of asymmetric) {
    tests.push((async () => {
      const { publicKey, privateKey } =
        await subtle.generateKey(algorithm, true, usages);
      assertSameSet(publicKey.usages, publicExpected,
                    `generateKey ${algorithm.name} publicKey`);
      assert.strictEqual(publicKey.usages.length, publicExpected.length);
      assertSameSet(privateKey.usages, privateExpected,
                    `generateKey ${algorithm.name} privateKey`);
      assert.strictEqual(privateKey.usages.length, privateExpected.length);
    })());
  }

  Promise.all(tests).then(common.mustCall());
}

{
  const tests = [];

  // Symmetric raw imports.
  const rawSymmetric = [
    { algorithm: { name: 'AES-CBC' }, keyData: new Uint8Array(16),
      usages: ['decrypt', 'encrypt', 'decrypt', 'encrypt'],
      expected: ['encrypt', 'decrypt'] },
    { algorithm: { name: 'AES-CTR' }, keyData: new Uint8Array(16),
      usages: ['wrapKey', 'encrypt', 'wrapKey', 'encrypt'],
      expected: ['encrypt', 'wrapKey'] },
    { algorithm: { name: 'AES-GCM' }, keyData: new Uint8Array(16),
      usages: ['decrypt', 'encrypt', 'decrypt'],
      expected: ['encrypt', 'decrypt'] },
    { algorithm: { name: 'HMAC', hash: 'SHA-256' }, keyData: new Uint8Array(32),
      usages: ['verify', 'sign', 'verify', 'sign'],
      expected: ['sign', 'verify'] },
  ];

  if (!process.features.openssl_is_boringssl) {
    rawSymmetric.push({
      algorithm: { name: 'AES-KW' }, keyData: new Uint8Array(16),
      usages: ['wrapKey', 'unwrapKey', 'wrapKey'],
      expected: ['wrapKey', 'unwrapKey'],
    });
  } else {
    common.printSkipMessage('AES-KW is not supported in BoringSSL');
  }

  if (hasOpenSSL(3)) {
    // KMAC does not support `raw` format, only `raw-secret` and `jwk`.
    tests.push((async () => {
      const key = await subtle.importKey(
        'raw-secret', new Uint8Array(16), { name: 'KMAC128' }, true,
        ['verify', 'sign', 'verify', 'sign']);
      assertSameSet(key.usages, ['sign', 'verify'],
                    'importKey raw-secret KMAC128');
      assert.strictEqual(key.usages.length, 2);
    })());

    tests.push((async () => {
      const jwk = {
        kty: 'oct',
        k: Buffer.from(new Uint8Array(16)).toString('base64url'),
        alg: 'K128',
      };
      const key = await subtle.importKey(
        'jwk', jwk, { name: 'KMAC128' }, true,
        ['verify', 'sign', 'verify', 'sign']);
      assertSameSet(key.usages, ['sign', 'verify'],
                    'importKey jwk KMAC128');
      assert.strictEqual(key.usages.length, 2);
    })());
  } else {
    common.printSkipMessage('AES-OCB and KMAC require OpenSSL >= 3');
  }

  for (const { algorithm, keyData, usages, expected } of rawSymmetric) {
    tests.push((async () => {
      const key = await subtle.importKey('raw', keyData, algorithm, true, usages);
      assertSameSet(key.usages, expected,
                    `importKey raw ${algorithm.name}`);
      assert.strictEqual(key.usages.length, expected.length);
    })());
  }

  // Generic secret keys (HKDF, PBKDF2) - importGenericSecretKey path.
  // These are not extractable.
  for (const name of ['HKDF', 'PBKDF2']) {
    tests.push((async () => {
      const key = await subtle.importKey(
        'raw',
        new Uint8Array(16),
        name,
        false,
        ['deriveBits', 'deriveKey', 'deriveBits', 'deriveKey']);
      assertSameSet(key.usages, ['deriveKey', 'deriveBits'],
                    `importKey raw ${name}`);
      assert.strictEqual(key.usages.length, 2);
    })());
  }

  // Argon2 - also via importGenericSecretKey, deriveBits-only.
  // Argon2 only supports raw-secret import.
  if (hasOpenSSL(3, 2)) {
    tests.push((async () => {
      const key = await subtle.importKey(
        'raw-secret',
        new Uint8Array(16),
        'Argon2id',
        false,
        ['deriveBits', 'deriveBits']);
      assertSameSet(key.usages, ['deriveBits'],
                    'importKey raw-secret Argon2id');
      assert.strictEqual(key.usages.length, 1);
    })());
  } else {
    common.printSkipMessage('Argon2 requires OpenSSL >= 3.2');
  }

  // JWK symmetric import.
  tests.push((async () => {
    const jwk = {
      kty: 'oct',
      k: 'AAAAAAAAAAAAAAAAAAAAAA',
      alg: 'A128CBC',
    };
    const key = await subtle.importKey('jwk', jwk, { name: 'AES-CBC' }, true,
                                       ['decrypt', 'encrypt', 'decrypt']);
    assertSameSet(key.usages, ['encrypt', 'decrypt'],
                  'importKey jwk AES-CBC');
    assert.strictEqual(key.usages.length, 2);
  })());

  // Asymmetric import via JWK - RSA, ECDSA, Ed25519.
  tests.push((async () => {
    // Generate, export, re-import with duplicate usages.
    const { privateKey } = await subtle.generateKey(
      { name: 'RSA-PSS', modulusLength: 2048,
        publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256' },
      true, ['sign', 'verify']);
    const jwk = await subtle.exportKey('jwk', privateKey);
    const imported = await subtle.importKey(
      'jwk', jwk,
      { name: 'RSA-PSS', hash: 'SHA-256' },
      true,
      ['sign', 'sign', 'sign']);
    assert.deepStrictEqual(imported.usages, ['sign']);
  })());

  tests.push((async () => {
    const { privateKey } = await subtle.generateKey(
      { name: 'ECDSA', namedCurve: 'P-256' },
      true, ['sign', 'verify']);
    const jwk = await subtle.exportKey('jwk', privateKey);
    const imported = await subtle.importKey(
      'jwk', jwk,
      { name: 'ECDSA', namedCurve: 'P-256' },
      true,
      ['sign', 'sign']);
    assert.deepStrictEqual(imported.usages, ['sign']);
  })());

  tests.push((async () => {
    const { privateKey } = await subtle.generateKey(
      { name: 'Ed25519' }, true, ['sign', 'verify']);
    const pkcs8 = await subtle.exportKey('pkcs8', privateKey);
    const imported = await subtle.importKey(
      'pkcs8', pkcs8,
      { name: 'Ed25519' },
      true,
      ['sign', 'sign', 'sign']);
    assert.deepStrictEqual(imported.usages, ['sign']);
  })());

  if (hasOpenSSL(3, 5)) {
    // ML-DSA JWK roundtrip.
    tests.push((async () => {
      const { privateKey } = await subtle.generateKey(
        { name: 'ML-DSA-65' }, true, ['sign', 'verify']);
      const jwk = await subtle.exportKey('jwk', privateKey);
      const imported = await subtle.importKey(
        'jwk', jwk, { name: 'ML-DSA-65' }, true,
        ['sign', 'sign', 'sign']);
      assert.deepStrictEqual(imported.usages, ['sign']);
    })());

    // ML-KEM JWK roundtrip.
    tests.push((async () => {
      const { privateKey } = await subtle.generateKey(
        { name: 'ML-KEM-768' }, true,
        ['decapsulateKey', 'decapsulateBits']);
      const jwk = await subtle.exportKey('jwk', privateKey);
      const imported = await subtle.importKey(
        'jwk', jwk, { name: 'ML-KEM-768' }, true,
        ['decapsulateBits', 'decapsulateKey',
         'decapsulateBits', 'decapsulateKey']);
      assert.deepStrictEqual(imported.usages,
                             ['decapsulateKey', 'decapsulateBits']);
    })());
  } else {
    common.printSkipMessage('ML-DSA and ML-KEM require OpenSSL >= 3.5');
  }

  // Spki import of RSA public key.
  tests.push((async () => {
    const { publicKey } = await subtle.generateKey(
      { name: 'RSA-OAEP', modulusLength: 2048,
        publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256' },
      true, ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey']);
    const spki = await subtle.exportKey('spki', publicKey);
    const imported = await subtle.importKey(
      'spki', spki,
      { name: 'RSA-OAEP', hash: 'SHA-256' },
      true,
      ['wrapKey', 'encrypt', 'wrapKey', 'encrypt']);
    assertSameSet(imported.usages, ['encrypt', 'wrapKey']);
    assert.strictEqual(imported.usages.length, 2);
  })());

  // ChaCha20-Poly1305 raw-secret import.
  if (!process.features.openssl_is_boringssl) {
    tests.push((async () => {
      const key = await subtle.importKey(
        'raw-secret',
        new Uint8Array(32),
        { name: 'ChaCha20-Poly1305' },
        true,
        ['decrypt', 'encrypt', 'decrypt', 'encrypt']);
      assertSameSet(key.usages, ['encrypt', 'decrypt']);
      assert.strictEqual(key.usages.length, 2);
    })());
  } else {
    common.printSkipMessage('ChaCha20-Poly1305 is not supported in BoringSSL');
  }

  // AES-OCB raw-secret import.
  if (hasOpenSSL(3)) {
    tests.push((async () => {
      const key = await subtle.importKey(
        'raw-secret',
        new Uint8Array(16),
        { name: 'AES-OCB' },
        true,
        ['decrypt', 'encrypt', 'decrypt', 'encrypt']);
      assertSameSet(key.usages, ['encrypt', 'decrypt']);
      assert.strictEqual(key.usages.length, 2);
    })());
  } else {
    common.printSkipMessage('AES-OCB requires OpenSSL >= 3');
  }

  Promise.all(tests).then(common.mustCall());
}

{
  const tests = [];

  // Symmetric: HMAC, AES-*, HKDF, PBKDF2
  tests.push((async () => {
    const keyObject = createSecretKey(new Uint8Array(32));
    const key = keyObject.toCryptoKey(
      { name: 'HMAC', hash: 'SHA-256' },
      true,
      ['verify', 'sign', 'verify', 'sign']);
    assertSameSet(key.usages, ['sign', 'verify']);
    assert.strictEqual(key.usages.length, 2);
  })());

  tests.push((async () => {
    const keyObject = createSecretKey(new Uint8Array(16));
    const key = keyObject.toCryptoKey(
      { name: 'AES-GCM' },
      true,
      ['decrypt', 'encrypt', 'decrypt']);
    assertSameSet(key.usages, ['encrypt', 'decrypt']);
    assert.strictEqual(key.usages.length, 2);
  })());

  tests.push((async () => {
    const keyObject = createSecretKey(new Uint8Array(32));
    const key = keyObject.toCryptoKey(
      'HKDF',
      false,
      ['deriveBits', 'deriveKey', 'deriveBits']);
    assertSameSet(key.usages, ['deriveKey', 'deriveBits']);
    assert.strictEqual(key.usages.length, 2);
  })());

  Promise.all(tests).then(common.mustCall());
}

{
  (async () => {
    const key = await subtle.generateKey(
      { name: 'AES-CTR', length: 128 },
      true,
      ['wrapKey', 'encrypt', 'decrypt', 'encrypt', 'wrapKey', 'unwrapKey']);
    // Regardless of the input order, de-duplicated usages are returned in
    // a canonical order.
    assert.deepStrictEqual(
      key.usages,
      ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey']);
  })().then(common.mustCall());
}

// Exported JWK `key_ops` must also be de-duplicated.
{
  const tests = [];

  const jwkVectors = [
    { algorithm: { name: 'HMAC', hash: 'SHA-256' },
      usages: ['verify', 'sign', 'verify', 'sign'],
      expected: ['sign', 'verify'] },
    { algorithm: { name: 'AES-CBC', length: 128 },
      usages: ['decrypt', 'encrypt', 'decrypt', 'encrypt'],
      expected: ['encrypt', 'decrypt'] },
    { algorithm: { name: 'AES-GCM', length: 128 },
      usages: ['decrypt', 'encrypt', 'decrypt'],
      expected: ['encrypt', 'decrypt'] },
  ];

  if (!process.features.openssl_is_boringssl) {
    jwkVectors.push({
      algorithm: { name: 'AES-KW', length: 128 },
      usages: ['wrapKey', 'unwrapKey', 'wrapKey', 'unwrapKey'],
      expected: ['wrapKey', 'unwrapKey'],
    });
  } else {
    common.printSkipMessage('AES-KW is not supported in BoringSSL');
  }

  if (hasOpenSSL(3)) {
    jwkVectors.push({
      algorithm: { name: 'AES-OCB', length: 128 },
      usages: ['decrypt', 'encrypt', 'decrypt', 'encrypt'],
      expected: ['encrypt', 'decrypt'],
    });
    jwkVectors.push({
      algorithm: { name: 'KMAC128', length: 128 },
      usages: ['verify', 'sign', 'verify', 'sign'],
      expected: ['sign', 'verify'],
    });
  } else {
    common.printSkipMessage('AES-OCB and KMAC require OpenSSL >= 3');
  }

  for (const { algorithm, usages, expected } of jwkVectors) {
    tests.push((async () => {
      const key = await subtle.generateKey(algorithm, true, usages);
      const jwk = await subtle.exportKey('jwk', key);
      assertSameSet(jwk.key_ops, expected,
                    `jwk key_ops for ${algorithm.name}`);
      assert.strictEqual(jwk.key_ops.length, expected.length,
                         `jwk key_ops length for ${algorithm.name}`);
    })());
  }

  const jwkPairVectors = [
    { algorithm: { name: 'RSA-OAEP', modulusLength: 2048,
                   publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256' },
      usages: ['wrapKey', 'unwrapKey', 'decrypt', 'encrypt',
               'unwrapKey', 'wrapKey', 'decrypt', 'encrypt'],
      publicExpected: ['encrypt', 'wrapKey'],
      privateExpected: ['decrypt', 'unwrapKey'] },
    { algorithm: { name: 'RSA-PSS', modulusLength: 2048,
                   publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256' },
      usages: ['verify', 'sign', 'verify', 'sign'],
      publicExpected: ['verify'],
      privateExpected: ['sign'] },
    { algorithm: { name: 'ECDSA', namedCurve: 'P-256' },
      usages: ['verify', 'sign', 'verify', 'sign', 'verify'],
      publicExpected: ['verify'],
      privateExpected: ['sign'] },
    { algorithm: { name: 'ECDH', namedCurve: 'P-256' },
      usages: ['deriveBits', 'deriveKey', 'deriveBits', 'deriveKey'],
      publicExpected: undefined,
      privateExpected: ['deriveKey', 'deriveBits'] },
    { algorithm: { name: 'Ed25519' },
      usages: ['verify', 'sign', 'verify', 'sign'],
      publicExpected: ['verify'],
      privateExpected: ['sign'] },
    { algorithm: { name: 'X25519' },
      usages: ['deriveBits', 'deriveKey', 'deriveBits', 'deriveKey'],
      publicExpected: undefined,
      privateExpected: ['deriveKey', 'deriveBits'] },
  ];

  if (hasOpenSSL(3, 5)) {
    jwkPairVectors.push({
      algorithm: { name: 'ML-DSA-65' },
      usages: ['verify', 'sign', 'verify', 'sign'],
      publicExpected: ['verify'],
      privateExpected: ['sign'],
    });
    jwkPairVectors.push({
      algorithm: { name: 'ML-KEM-768' },
      usages: ['decapsulateBits', 'encapsulateBits', 'decapsulateKey',
               'encapsulateKey', 'decapsulateBits', 'encapsulateBits'],
      publicExpected: ['encapsulateKey', 'encapsulateBits'],
      privateExpected: ['decapsulateKey', 'decapsulateBits'],
    });
  } else {
    common.printSkipMessage('ML-DSA and ML-KEM require OpenSSL >= 3.5');
  }

  for (const { algorithm, usages, publicExpected, privateExpected } of jwkPairVectors) {
    tests.push((async () => {
      const { publicKey, privateKey } =
        await subtle.generateKey(algorithm, true, usages);
      const publicJwk = await subtle.exportKey('jwk', publicKey);
      const privateJwk = await subtle.exportKey('jwk', privateKey);

      if (publicExpected === undefined) {
        // Empty public-key usages result in an empty `key_ops`.
        assert.deepStrictEqual(publicJwk.key_ops, [],
                               `jwk key_ops for ${algorithm.name} publicKey`);
      } else {
        assertSameSet(publicJwk.key_ops, publicExpected,
                      `jwk key_ops for ${algorithm.name} publicKey`);
        assert.strictEqual(publicJwk.key_ops.length, publicExpected.length);
      }

      assertSameSet(privateJwk.key_ops, privateExpected,
                    `jwk key_ops for ${algorithm.name} privateKey`);
      assert.strictEqual(privateJwk.key_ops.length, privateExpected.length);
    })());
  }

  Promise.all(tests).then(common.mustCall());
}
