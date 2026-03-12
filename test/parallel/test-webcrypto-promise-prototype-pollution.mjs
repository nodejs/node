import * as common from '../common/index.mjs';

if (!common.hasCrypto) common.skip('missing crypto');

// WebCrypto subtle methods must not leak intermediate values
// through Promise.prototype.then pollution.
// Regression test for https://github.com/nodejs/node/pull/61492
// and https://github.com/nodejs/node/issues/59699.

import { hasOpenSSL } from '../common/crypto.js';

const { subtle } = globalThis.crypto;

Promise.prototype.then = common.mustNotCall('Promise.prototype.then');

await subtle.digest('SHA-256', new Uint8Array([1, 2, 3]));

await subtle.generateKey({ name: 'AES-CBC', length: 256 }, true, ['encrypt', 'decrypt']);

await subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign', 'verify']);

const rawKey = globalThis.crypto.getRandomValues(new Uint8Array(32));

const importedKey = await subtle.importKey(
  'raw', rawKey, { name: 'AES-CBC', length: 256 }, false, ['encrypt', 'decrypt']);

const exportableKey = await subtle.importKey(
  'raw', rawKey, { name: 'AES-CBC', length: 256 }, true, ['encrypt', 'decrypt']);

await subtle.exportKey('raw', exportableKey);

const iv = globalThis.crypto.getRandomValues(new Uint8Array(16));
const plaintext = new TextEncoder().encode('Hello, world!');

const ciphertext = await subtle.encrypt({ name: 'AES-CBC', iv }, importedKey, plaintext);

await subtle.decrypt({ name: 'AES-CBC', iv }, importedKey, ciphertext);

const signingKey = await subtle.generateKey(
  { name: 'HMAC', hash: 'SHA-256' }, false, ['sign', 'verify']);

const data = new TextEncoder().encode('test data');

const signature = await subtle.sign('HMAC', signingKey, data);

await subtle.verify('HMAC', signingKey, signature, data);

const pbkdf2Key = await subtle.importKey(
  'raw', rawKey, 'PBKDF2', false, ['deriveBits', 'deriveKey']);

await subtle.deriveBits(
  { name: 'PBKDF2', salt: rawKey, iterations: 1000, hash: 'SHA-256' },
  pbkdf2Key, 256);

await subtle.deriveKey(
  { name: 'PBKDF2', salt: rawKey, iterations: 1000, hash: 'SHA-256' },
  pbkdf2Key,
  { name: 'AES-CBC', length: 256 },
  true,
  ['encrypt', 'decrypt']);

const wrappingKey = await subtle.generateKey(
  { name: 'AES-KW', length: 256 }, true, ['wrapKey', 'unwrapKey']);

const keyToWrap = await subtle.generateKey(
  { name: 'AES-CBC', length: 256 }, true, ['encrypt', 'decrypt']);

const wrapped = await subtle.wrapKey('raw', keyToWrap, wrappingKey, 'AES-KW');

await subtle.unwrapKey(
  'raw', wrapped, wrappingKey, 'AES-KW',
  { name: 'AES-CBC', length: 256 }, true, ['encrypt', 'decrypt']);

const { privateKey } = await subtle.generateKey(
  { name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign', 'verify']);

await subtle.getPublicKey(privateKey, ['verify']);

if (hasOpenSSL(3, 5)) {
  const kemPair = await subtle.generateKey(
    { name: 'ML-KEM-768' }, false,
    ['encapsulateKey', 'encapsulateBits', 'decapsulateKey', 'decapsulateBits']);

  const { ciphertext: ct1 } = await subtle.encapsulateKey(
    { name: 'ML-KEM-768' }, kemPair.publicKey, 'HKDF', false, ['deriveBits']);

  await subtle.decapsulateKey(
    { name: 'ML-KEM-768' }, kemPair.privateKey, ct1, 'HKDF', false, ['deriveBits']);

  const { ciphertext: ct2 } = await subtle.encapsulateBits(
    { name: 'ML-KEM-768' }, kemPair.publicKey);

  await subtle.decapsulateBits(
    { name: 'ML-KEM-768' }, kemPair.privateKey, ct2);
}
