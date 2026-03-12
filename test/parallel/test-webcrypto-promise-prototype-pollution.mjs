import * as common from '../common/index.mjs';

if (!common.hasCrypto) common.skip('missing crypto');

// WebCrypto subtle methods must not leak intermediate values
// through Promise.prototype.then pollution.
// Regression test for https://github.com/nodejs/node/pull/61492
// and https://github.com/nodejs/node/issues/59699.

import * as assert from 'node:assert/strict';
import { hasOpenSSL } from '../common/crypto.js';

const { subtle } = globalThis.crypto;

const originalThen = Promise.prototype.then;
const intercepted = [];

// Pollute Promise.prototype.then to record all resolved values.
Promise.prototype.then = function(onFulfilled, ...rest) {
  return originalThen.call(this, function(value) {
    intercepted.push(value);
    return typeof onFulfilled === 'function' ? onFulfilled(value) : value;
  }, ...rest);
};

async function test(label, fn) {
  const result = await fn();
  assert.strictEqual(
    intercepted.length, 0,
    `Promise.prototype.then was called during ${label}`
  );
  return result;
}

await test('digest', () =>
  subtle.digest('SHA-256', new Uint8Array([1, 2, 3])));

await test('generateKey (AES-CBC)', () =>
  subtle.generateKey({ name: 'AES-CBC', length: 256 }, true, ['encrypt', 'decrypt']));

await test('generateKey (ECDSA)', () =>
  subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign', 'verify']));

const rawKey = globalThis.crypto.getRandomValues(new Uint8Array(32));

const importedKey = await test('importKey', () =>
  subtle.importKey('raw', rawKey, { name: 'AES-CBC', length: 256 }, false, ['encrypt', 'decrypt']));

const exportableKey = await test('importKey (extractable)', () =>
  subtle.importKey('raw', rawKey, { name: 'AES-CBC', length: 256 }, true, ['encrypt', 'decrypt']));

await test('exportKey', () =>
  subtle.exportKey('raw', exportableKey));

const iv = globalThis.crypto.getRandomValues(new Uint8Array(16));
const plaintext = new TextEncoder().encode('Hello, world!');

const ciphertext = await test('encrypt', () =>
  subtle.encrypt({ name: 'AES-CBC', iv }, importedKey, plaintext));

await test('decrypt', () =>
  subtle.decrypt({ name: 'AES-CBC', iv }, importedKey, ciphertext));

const signingKey = await test('generateKey (HMAC)', () =>
  subtle.generateKey({ name: 'HMAC', hash: 'SHA-256' }, false, ['sign', 'verify']));

const data = new TextEncoder().encode('test data');

const signature = await test('sign', () =>
  subtle.sign('HMAC', signingKey, data));

await test('verify', () =>
  subtle.verify('HMAC', signingKey, signature, data));

const pbkdf2Key = await test('importKey (PBKDF2)', () =>
  subtle.importKey('raw', rawKey, 'PBKDF2', false, ['deriveBits', 'deriveKey']));

await test('deriveBits', () =>
  subtle.deriveBits(
    { name: 'PBKDF2', salt: rawKey, iterations: 1000, hash: 'SHA-256' },
    pbkdf2Key, 256));

// deriveKey — this was the primary leak reported in the issue
await test('deriveKey', () =>
  subtle.deriveKey(
    { name: 'PBKDF2', salt: rawKey, iterations: 1000, hash: 'SHA-256' },
    pbkdf2Key,
    { name: 'AES-CBC', length: 256 },
    true,
    ['encrypt', 'decrypt']));

const wrappingKey = await test('generateKey (AES-KW)', () =>
  subtle.generateKey({ name: 'AES-KW', length: 256 }, true, ['wrapKey', 'unwrapKey']));

const keyToWrap = await test('generateKey (AES-CBC for wrap)', () =>
  subtle.generateKey({ name: 'AES-CBC', length: 256 }, true, ['encrypt', 'decrypt']));

const wrapped = await test('wrapKey', () =>
  subtle.wrapKey('raw', keyToWrap, wrappingKey, 'AES-KW'));

await test('unwrapKey', () =>
  subtle.unwrapKey(
    'raw', wrapped, wrappingKey, 'AES-KW',
    { name: 'AES-CBC', length: 256 }, true, ['encrypt', 'decrypt']));

const { privateKey } = await test('generateKey (ECDSA for getPublicKey)', () =>
  subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign', 'verify']));

await test('getPublicKey', () =>
  subtle.getPublicKey(privateKey, ['verify']));

if (hasOpenSSL(3, 5)) {
  const kemPair = await test('generateKey (ML-KEM-768)', () =>
    subtle.generateKey(
      { name: 'ML-KEM-768' }, false,
      ['encapsulateKey', 'encapsulateBits', 'decapsulateKey', 'decapsulateBits']));

  const { ciphertext: ct1 } = await test('encapsulateKey', () =>
    subtle.encapsulateKey(
      { name: 'ML-KEM-768' }, kemPair.publicKey, 'HKDF', false, ['deriveBits']));

  await test('decapsulateKey', () =>
    subtle.decapsulateKey(
      { name: 'ML-KEM-768' }, kemPair.privateKey, ct1, 'HKDF', false, ['deriveBits']));

  const { ciphertext: ct2 } = await test('encapsulateBits', () =>
    subtle.encapsulateBits({ name: 'ML-KEM-768' }, kemPair.publicKey));

  await test('decapsulateBits', () =>
    subtle.decapsulateBits({ name: 'ML-KEM-768' }, kemPair.privateKey, ct2));
}
