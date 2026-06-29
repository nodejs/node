'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 5))
  common.skip('requires OpenSSL >= 3.5');

const assert = require('assert');
const crypto = require('crypto');
const { KeyObject } = crypto;
const { subtle } = globalThis.crypto;

const vectors = require('../fixtures/crypto/ml-kem')();

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

  assert(encapsulated2.sharedKey instanceof CryptoKey);
  assert.strictEqual(encapsulated2.sharedKey.algorithm.name, 'HMAC');
  assert.strictEqual(encapsulated2.sharedKey.extractable, false);

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

  // Verify the keys are the same by using KeyObject.from() and comparing
  const originalKeyData = KeyObject.from(encapsulated.sharedKey).export();
  const decapsulatedKeyData = KeyObject.from(decapsulatedKey).export();
  assert(originalKeyData.equals(decapsulatedKeyData));

  // Test with test vector ciphertext and expected shared key
  const vectorDecapsulatedKey = await subtle.decapsulateKey(
    { name },
    privateKey,
    results.ciphertext,
    'HKDF',
    false,
    ['deriveBits']
  );

  const vectorKeyData = KeyObject.from(vectorDecapsulatedKey).export();
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

  vectors.forEach((vector) => {
    variations.push(testEncapsulateKey(vector));
    variations.push(testEncapsulateBits(vector));
    variations.push(testDecapsulateKey(vector));
    variations.push(testDecapsulateBits(vector));
  });

  await Promise.all(variations);
})().then(common.mustCall());
