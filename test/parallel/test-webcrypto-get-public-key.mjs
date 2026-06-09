// Flags: --expose-internals

import * as common from '../common/index.mjs';

if (!common.hasCrypto) common.skip('missing crypto');

import * as assert from 'node:assert';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { kSupportedAlgorithms } = require('internal/crypto/util');
const { SubtleCrypto } = globalThis;
const { subtle } = globalThis.crypto;

const RSA_KEY_GEN = {
  modulusLength: 2048,
  publicExponent: new Uint8Array([1, 0, 1]),
  hash: 'SHA-256',
};

function vector(algorithm, privateUsages, publicUsages) {
  return { algorithm, privateUsages, publicUsages };
}

const keyGeneration = {
  'ECDH': vector({ name: 'ECDH', namedCurve: 'P-256' }, ['deriveBits'], []),
  'ECDSA': vector({ name: 'ECDSA', namedCurve: 'P-256' }, ['sign'], ['verify']),
  'RSA-OAEP': vector(
    { name: 'RSA-OAEP', ...RSA_KEY_GEN },
    ['decrypt', 'unwrapKey'],
    ['encrypt', 'wrapKey']),
  'RSA-PSS': vector({ name: 'RSA-PSS', ...RSA_KEY_GEN }, ['sign'], ['verify']),
  'RSASSA-PKCS1-v1_5': vector(
    { name: 'RSASSA-PKCS1-v1_5', ...RSA_KEY_GEN },
    ['sign'],
    ['verify']),
};

for (const name of [
  'Ed25519',
  'Ed448',
  'ML-DSA-44',
  'ML-DSA-65',
  'ML-DSA-87',
]) {
  keyGeneration[name] = vector(name, ['sign'], ['verify']);
}

for (const name of ['X25519', 'X448']) {
  keyGeneration[name] = vector(name, ['deriveBits'], []);
}

for (const name of ['ML-KEM-512', 'ML-KEM-768', 'ML-KEM-1024']) {
  keyGeneration[name] = vector(name, ['decapsulateBits'], ['encapsulateBits']);
}

const unsupportedGetPublicKeyAlgorithms = new Set([
  'AES-CBC',
  'AES-CTR',
  'AES-GCM',
  'AES-KW',
  'AES-OCB',
  'ChaCha20-Poly1305',
  'HMAC',
  'KMAC128',
  'KMAC256',
]);

for (const name of Object.keys(kSupportedAlgorithms.exportKey)) {
  const test = keyGeneration[name];
  if (test === undefined) {
    if (!unsupportedGetPublicKeyAlgorithms.has(name)) {
      assert.fail(
        `${name} needs a keyGeneration entry or must be listed in ` +
        'unsupportedGetPublicKeyAlgorithms');
    }
    assert.strictEqual(SubtleCrypto.supports('getPublicKey', name), false);
    continue;
  }

  assert.strictEqual(SubtleCrypto.supports('getPublicKey', name), true);

  const { privateKey } = await subtle.generateKey(
    test.algorithm, false, test.privateUsages);
  const usages = test.publicUsages;
  const publicKey = await subtle.getPublicKey(privateKey, usages);
  assert.deepStrictEqual(publicKey.algorithm, privateKey.algorithm);
  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(publicKey.extractable, true);

  await assert.rejects(() => subtle.getPublicKey(privateKey, ['deriveBits']), {
    name: 'SyntaxError',
    message: /Unsupported key usage/
  });

  await assert.rejects(() => subtle.getPublicKey(publicKey, publicKey.usages), {
    name: 'InvalidAccessError',
    message: 'key must be a private key'
  });
}

const secretKey = await subtle.generateKey(
  { name: 'AES-CBC', length: 128 }, true, ['encrypt', 'decrypt']);
await assert.rejects(
  () => subtle.getPublicKey(secretKey, ['encrypt', 'decrypt']),
  {
    name: 'NotSupportedError',
    message: 'key must be a private key'
  });
