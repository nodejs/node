'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (!process.features.openssl_is_boringssl)
  common.skip('Skipping unsupported ChaCha20-Poly1305 test case');

const assert = require('assert');
const { subtle } = globalThis.crypto;

async function testEncrypt({ keyBuffer, algorithm, plaintext, result }) {
  // Using a copy of plaintext to prevent tampering of the original
  plaintext = Buffer.from(plaintext);

  const key = await subtle.importKey(
    'raw-secret',
    keyBuffer,
    { name: algorithm.name },
    false,
    ['encrypt', 'decrypt']);

  const output = await subtle.encrypt(algorithm, key, plaintext);
  plaintext[0] = 255 - plaintext[0];

  assert.strictEqual(
    Buffer.from(output).toString('hex'),
    Buffer.from(result).toString('hex'));

  // Converting the returned ArrayBuffer into a Buffer right away,
  // so that the next line works
  const check = Buffer.from(await subtle.decrypt(algorithm, key, output));
  check[0] = 255 - check[0];

  assert.strictEqual(
    Buffer.from(check).toString('hex'),
    Buffer.from(plaintext).toString('hex'));
}

async function testEncryptNoEncrypt({ keyBuffer, algorithm, plaintext }) {
  const key = await subtle.importKey(
    'raw-secret',
    keyBuffer,
    { name: algorithm.name },
    false,
    ['decrypt']);

  return assert.rejects(subtle.encrypt(algorithm, key, plaintext), {
    message: /The requested operation is not valid for the provided key/
  });
}

async function testEncryptNoDecrypt({ keyBuffer, algorithm, plaintext }) {
  const key = await subtle.importKey(
    'raw-secret',
    keyBuffer,
    { name: algorithm.name },
    false,
    ['encrypt']);

  const output = await subtle.encrypt(algorithm, key, plaintext);

  return assert.rejects(subtle.decrypt(algorithm, key, output), {
    message: /The requested operation is not valid for the provided key/
  });
}

async function testEncryptWrongAlg({ keyBuffer, algorithm, plaintext }, alg) {
  assert.notStrictEqual(algorithm.name, alg);
  const key = await subtle.importKey(
    'raw-secret',
    keyBuffer,
    { name: alg },
    false,
    ['encrypt']);

  return assert.rejects(subtle.encrypt(algorithm, key, plaintext), {
    message: /The requested operation is not valid for the provided key/
  });
}

async function testDecrypt({ keyBuffer, algorithm, result }) {
  const key = await subtle.importKey(
    'raw-secret',
    keyBuffer,
    { name: algorithm.name },
    false,
    ['encrypt', 'decrypt']);

  await subtle.decrypt(algorithm, key, result);
}

{
  const {
    passing,
    failing,
    decryptionFailing
  } = require('../fixtures/crypto/chacha20_poly1305')();

  (async function() {
    const variations = [];

    passing.forEach((vector) => {
      variations.push(testEncrypt(vector));
      variations.push(testEncryptNoEncrypt(vector));
      variations.push(testEncryptNoDecrypt(vector));
      variations.push(testEncryptWrongAlg(vector, 'AES-GCM'));
    });

    failing.forEach((vector) => {
      variations.push(assert.rejects(testEncrypt(vector), {
        message: /is not a valid ChaCha20-Poly1305 tag length/
      }));
      variations.push(assert.rejects(testDecrypt(vector), {
        message: /is not a valid ChaCha20-Poly1305 tag length/
      }));
    });

    decryptionFailing.forEach((vector) => {
      variations.push(assert.rejects(testDecrypt(vector), {
        name: 'OperationError'
      }));
    });

    await Promise.all(variations);
  })().then(common.mustCall());
}

{
  (async function() {
    const secretKey = await subtle.generateKey(
      {
        name: 'ChaCha20-Poly1305',
      },
      false,
      ['encrypt', 'decrypt'],
    );

    const iv = globalThis.crypto.getRandomValues(new Uint8Array(12));
    const aad = globalThis.crypto.getRandomValues(new Uint8Array(32));

    const encrypted = await subtle.encrypt(
      {
        name: 'ChaCha20-Poly1305',
        iv,
        additionalData: aad,
      },
      secretKey,
      globalThis.crypto.getRandomValues(new Uint8Array(32))
    );

    await subtle.decrypt(
      {
        name: 'ChaCha20-Poly1305',
        iv,
        additionalData: aad,
      },
      secretKey,
      new Uint8Array(encrypted),
    );
  })().then(common.mustCall());
}

{

  async function testRejectsImportKey(format, keyData, algorithm, extractable, usages, expectedError) {
    await assert.rejects(
      subtle.importKey(format, keyData, algorithm, extractable, usages),
      expectedError
    );
  }

  async function testRejectsGenerateKey(algorithm, extractable, usages, expectedError) {
    await assert.rejects(
      subtle.generateKey(algorithm, extractable, usages),
      expectedError
    );
  }

  (async function() {
    const baseJwk = { kty: 'oct', k: 'AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA' };
    const alg = { name: 'ChaCha20-Poly1305' };
    const keyData32 = globalThis.crypto.getRandomValues(new Uint8Array(32));

    // Test decrypt with data too small
    const secretKey = await subtle.generateKey(alg, false, ['encrypt', 'decrypt']);
    const iv = globalThis.crypto.getRandomValues(new Uint8Array(12));
    await assert.rejects(
      subtle.decrypt({ name: 'ChaCha20-Poly1305', iv }, secretKey, new Uint8Array(8)),
      { name: 'OperationError', message: /The provided data is too small/ }
    );

    // Test invalid tagLength values
    await assert.rejects(
      subtle.encrypt({ name: 'ChaCha20-Poly1305', iv, tagLength: 64 }, secretKey, keyData32),
      { name: 'OperationError', message: /is not a valid ChaCha20-Poly1305 tag length/ }
    );
    await assert.rejects(
      subtle.encrypt({ name: 'ChaCha20-Poly1305', iv, tagLength: 96 }, secretKey, keyData32),
      { name: 'OperationError', message: /is not a valid ChaCha20-Poly1305 tag length/ }
    );

    // JWK error conditions
    const jwkTests = [
      [{ k: baseJwk.k }, /Invalid keyData/],
      [{ ...baseJwk, kty: 'RSA' }, /Invalid JWK "kty" Parameter/],
      [{ ...baseJwk, use: 'sig' }, /Invalid JWK "use" Parameter/],
      [{ ...baseJwk, ext: false }, /JWK "ext" Parameter and extractable mismatch/, true],
      [{ ...baseJwk, alg: 'A256GCM' }, /JWK "alg" does not match the requested algorithm/],
      [{ ...baseJwk, key_ops: ['sign'] }, /Key operations and usage mismatch|Unsupported key usage/],
      [{ ...baseJwk, key_ops: ['encrypt'] }, /Key operations and usage mismatch/, false, ['decrypt']],
    ];

    for (const [jwk, errorPattern, extractable = false, usages = ['encrypt']] of jwkTests) {
      await testRejectsImportKey('jwk', jwk, alg, extractable, usages,
                                 { name: 'DataError', message: errorPattern });
    }

    // Valid JWK imports
    const validKeys = await Promise.all([
      subtle.importKey('jwk', { ...baseJwk, alg: 'C20P' }, alg, false, ['encrypt']),
      subtle.importKey('jwk', { ...baseJwk, use: 'enc' }, alg, false, ['encrypt']),
    ]);
    validKeys.forEach((key) => assert.strictEqual(key.algorithm.name, 'ChaCha20-Poly1305'));

    // Invalid key usages
    const usageTests = [
      [['sign'], 'generateKey'],
      [['verify'], 'importKey'],
    ];

    for (const [usages, method] of usageTests) {
      const fn = method === 'generateKey' ?
        () => testRejectsGenerateKey(alg, false, usages, { name: 'SyntaxError', message: /Unsupported key usage/ }) :
        () => testRejectsImportKey('raw-secret', keyData32, alg, false, usages, { name: 'SyntaxError', message: /Unsupported key usage/ });
      await fn();
    }

    // Valid wrapKey/unwrapKey usage
    const wrapKey = await subtle.importKey('raw-secret', keyData32, alg, false, ['wrapKey', 'unwrapKey']);
    assert.strictEqual(wrapKey.algorithm.name, 'ChaCha20-Poly1305');

    // Invalid key lengths
    for (const size of [16, 64]) {
      await testRejectsImportKey('raw-secret', new Uint8Array(size), alg, false, ['encrypt'],
                                 { name: 'DataError', message: /Invalid key length/ });
    }

    // Invalid JWK keyData
    await testRejectsImportKey('jwk', { ...baseJwk, k: 'invalid-base64-!@#$%^&*()' }, alg, false, ['encrypt'],
                               { name: 'DataError' });
  })().then(common.mustCall());
}
