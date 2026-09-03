// Flags: --expose-internals
'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 5) && !process.features.openssl_is_boringssl)
  common.skip('requires OpenSSL >= 3.5 or BoringSSL');

const assert = require('assert');
const crypto = require('crypto');
const { getCryptoKeyHandle } = require('internal/crypto/keys');
const { subtle } = globalThis.crypto;

const vectors = require('../fixtures/crypto/ml-kem')();

function getCryptoKeyData(key) {
  return getCryptoKeyHandle(key).export();
}

async function testEncapsulateKey({ name, publicKeyPem, privateKeyPem, results }) {
  const [
    publicKey,
    noEncapsulatePublicKey,
    privateKey,
  ] = await Promise.all([
    crypto.createPublicKey(publicKeyPem)
      .toCryptoKey(name, false, ['encapsulateKey']),
    crypto.createPublicKey(publicKeyPem)
      .toCryptoKey(name, false, ['encapsulateBits']),
    crypto.createPrivateKey(privateKeyPem)
      .toCryptoKey(name, false, ['decapsulateKey']),
  ]);

  // Test successful encapsulation
  const encapsulated = await subtle.encapsulateKey(
    { name },
    publicKey,
    'HKDF',
    false,
    ['deriveBits']
  );

  assert.strictEqual(Object.getPrototypeOf(encapsulated), Object.prototype);
  assert(encapsulated.sharedKey instanceof CryptoKey);
  assert(encapsulated.ciphertext instanceof ArrayBuffer);
  assert.strictEqual(encapsulated.sharedKey.type, 'secret');
  assert.strictEqual(encapsulated.sharedKey.algorithm.name, 'HKDF');
  assert.strictEqual(encapsulated.sharedKey.extractable, false);
  assert.deepStrictEqual(encapsulated.sharedKey.usages, ['deriveBits']);

  // Verify ciphertext length matches expected for algorithm
  assert.strictEqual(encapsulated.ciphertext.byteLength, results.ciphertext.byteLength);

  // Test with different shared key algorithm
  const encapsulated2 = await subtle.encapsulateKey(
    { name },
    publicKey,
    { name: 'HMAC', hash: 'SHA-256' },
    false,
    ['sign', 'verify']
  );

  assert.strictEqual(Object.getPrototypeOf(encapsulated2), Object.prototype);
  assert(encapsulated2.sharedKey instanceof CryptoKey);
  assert.strictEqual(encapsulated2.sharedKey.algorithm.name, 'HMAC');
  assert.strictEqual(encapsulated2.sharedKey.extractable, false);

  const encapsulated3 = await subtle.encapsulateKey(
    { name },
    publicKey,
    { name: 'HMAC', hash: 'SHA-256', length: 255 },
    false,
    ['sign', 'verify']
  );

  assert.strictEqual(encapsulated3.sharedKey.algorithm.name, 'HMAC');
  assert.strictEqual(encapsulated3.sharedKey.algorithm.length, 255);
  assert.strictEqual(getCryptoKeyData(encapsulated3.sharedKey).length, 32);
  assert.strictEqual(getCryptoKeyData(encapsulated3.sharedKey)[31] & 0b00000001, 0);

  // Test failure when using wrong key type
  await assert.rejects(
    subtle.encapsulateKey({ name }, privateKey, 'HKDF', false, ['deriveBits']), {
      name: 'InvalidAccessError',
    });

  // Test failure when using key without proper usage
  await assert.rejects(
    subtle.encapsulateKey({ name }, noEncapsulatePublicKey, 'HKDF', false, ['deriveBits']), {
      name: 'InvalidAccessError',
    });
}

async function testEncapsulateBits({ name, publicKeyPem, privateKeyPem, results }) {
  const [
    publicKey,
    noEncapsulatePublicKey,
    privateKey,
  ] = await Promise.all([
    crypto.createPublicKey(publicKeyPem)
      .toCryptoKey(name, false, ['encapsulateBits']),
    crypto.createPublicKey(publicKeyPem)
      .toCryptoKey(name, false, ['encapsulateKey']),
    crypto.createPrivateKey(privateKeyPem)
      .toCryptoKey(name, false, ['decapsulateBits']),
  ]);

  // Test successful encapsulation
  const encapsulated = await subtle.encapsulateBits({ name }, publicKey);

  assert.strictEqual(Object.getPrototypeOf(encapsulated), Object.prototype);
  assert(encapsulated.sharedKey instanceof ArrayBuffer);
  assert(encapsulated.ciphertext instanceof ArrayBuffer);
  assert.strictEqual(encapsulated.sharedKey.byteLength, 32); // ML-KEM shared secret is 32 bytes

  // Verify ciphertext length matches expected for algorithm
  assert.strictEqual(encapsulated.ciphertext.byteLength, results.ciphertext.byteLength);

  // Test failure when using wrong key type
  await assert.rejects(
    subtle.encapsulateBits({ name }, privateKey), {
      name: 'InvalidAccessError',
    });

  // Test failure when using key without proper usage
  await assert.rejects(
    subtle.encapsulateBits({ name }, noEncapsulatePublicKey), {
      name: 'InvalidAccessError',
    });
}

async function testDecapsulateKey({ name, publicKeyPem, privateKeyPem, results }) {
  const [
    publicKey,
    privateKey,
    noDecapsulatePrivateKey,
  ] = await Promise.all([
    crypto.createPublicKey(publicKeyPem)
      .toCryptoKey(name, false, ['encapsulateKey']),
    crypto.createPrivateKey(privateKeyPem)
      .toCryptoKey(name, false, ['decapsulateKey']),
    crypto.createPrivateKey(privateKeyPem)
      .toCryptoKey(name, false, ['decapsulateBits']),
  ]);

  // Test successful round-trip: encapsulate then decapsulate
  const encapsulated = await subtle.encapsulateKey(
    { name },
    publicKey,
    'HKDF',
    false,
    ['deriveBits']
  );

  const decapsulatedKey = await subtle.decapsulateKey(
    { name },
    privateKey,
    encapsulated.ciphertext,
    'HKDF',
    false,
    ['deriveBits']
  );

  assert(decapsulatedKey instanceof CryptoKey);
  assert.strictEqual(decapsulatedKey.type, 'secret');
  assert.strictEqual(decapsulatedKey.algorithm.name, 'HKDF');
  assert.strictEqual(decapsulatedKey.extractable, false);
  assert.deepStrictEqual(decapsulatedKey.usages, ['deriveBits']);

  // Verify the non-extractable keys are the same.
  const originalKeyData = getCryptoKeyData(encapsulated.sharedKey);
  const decapsulatedKeyData = getCryptoKeyData(decapsulatedKey);
  assert(originalKeyData.equals(decapsulatedKeyData));

  const decapsulatedHmac = await subtle.decapsulateKey(
    { name },
    privateKey,
    encapsulated.ciphertext,
    { name: 'HMAC', hash: 'SHA-256', length: 255 },
    false,
    ['sign', 'verify']
  );
  assert.strictEqual(decapsulatedHmac.algorithm.name, 'HMAC');
  assert.strictEqual(decapsulatedHmac.algorithm.length, 255);
  assert.strictEqual(getCryptoKeyData(decapsulatedHmac).length, 32);
  assert.strictEqual(getCryptoKeyData(decapsulatedHmac)[31] & 0b00000001, 0);

  // Test with test vector ciphertext and expected shared key
  const vectorDecapsulatedKey = await subtle.decapsulateKey(
    { name },
    privateKey,
    results.ciphertext,
    'HKDF',
    false,
    ['deriveBits']
  );

  const vectorKeyData = getCryptoKeyData(vectorDecapsulatedKey);
  assert(vectorKeyData.equals(results.sharedKey));

  // Test failure when using wrong key type
  await assert.rejects(
    subtle.decapsulateKey({ name }, publicKey, encapsulated.ciphertext,
                          'HKDF', false, ['deriveKey']), {
      name: 'InvalidAccessError'
    });

  // Test failure when using key without proper usage
  await assert.rejects(
    subtle.decapsulateKey({ name }, noDecapsulatePrivateKey, encapsulated.ciphertext,
                          'HKDF', false, ['deriveKey']), {
      name: 'InvalidAccessError'
    });

  // Test failure with wrong ciphertext length
  const wrongLengthCiphertext = new Uint8Array(encapsulated.ciphertext.byteLength - 1);
  await assert.rejects(
    subtle.decapsulateKey({ name }, privateKey, wrongLengthCiphertext,
                          'HKDF', false, ['deriveKey']), {
      name: 'OperationError',
    });
}

async function testDecapsulateBits({ name, publicKeyPem, privateKeyPem, results }) {
  const [
    publicKey,
    privateKey,
    noDecapsulatePrivateKey,
  ] = await Promise.all([
    crypto.createPublicKey(publicKeyPem)
      .toCryptoKey(name, false, ['encapsulateBits']),
    crypto.createPrivateKey(privateKeyPem)
      .toCryptoKey(name, false, ['decapsulateBits']),
    crypto.createPrivateKey(privateKeyPem)
      .toCryptoKey(name, false, ['decapsulateKey']),
  ]);

  // Test successful round-trip: encapsulate then decapsulate
  const encapsulated = await subtle.encapsulateBits({ name }, publicKey);

  const decapsulatedBits = await subtle.decapsulateBits(
    { name },
    privateKey,
    encapsulated.ciphertext
  );

  assert(decapsulatedBits instanceof ArrayBuffer);
  assert.strictEqual(decapsulatedBits.byteLength, 32); // ML-KEM shared secret is 32 bytes

  // Verify the shared secrets are the same
  assert(Buffer.from(encapsulated.sharedKey).equals(Buffer.from(decapsulatedBits)));

  // Test with test vector ciphertext and expected shared key
  const vectorDecapsulatedBits = await subtle.decapsulateBits(
    { name },
    privateKey,
    results.ciphertext
  );

  assert(Buffer.from(vectorDecapsulatedBits).equals(results.sharedKey));

  // Test failure when using wrong key type
  await assert.rejects(
    subtle.decapsulateBits({ name }, publicKey, encapsulated.ciphertext), {
      name: 'InvalidAccessError'
    });

  // Test failure when using key without proper usage
  await assert.rejects(
    subtle.decapsulateBits({ name }, noDecapsulatePrivateKey, encapsulated.ciphertext), {
      name: 'InvalidAccessError'
    });

  // Test failure with wrong ciphertext length
  const wrongLengthCiphertext = new Uint8Array(encapsulated.ciphertext.byteLength - 1);
  await assert.rejects(
    subtle.decapsulateBits({ name }, privateKey, wrongLengthCiphertext), {
      name: 'OperationError',
    });
}

(async function() {
  const variations = [];

  for (const vector of vectors) {
    if (process.features.openssl_is_boringssl && vector.name === 'ML-KEM-512') {
      common.printSkipMessage(`Skipping unsupported ${vector.name} test`);
      continue;
    }
    variations.push(testEncapsulateKey(vector));
    variations.push(testEncapsulateBits(vector));
    variations.push(testDecapsulateKey(vector));
    variations.push(testDecapsulateBits(vector));
  }

  await Promise.all(variations);
})().then(common.mustCall());
