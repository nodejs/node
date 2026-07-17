'use strict';

// CryptoKey instances must not expose any own Symbol-keyed properties
// to user code. The internal slots backing the public getters are
// kept in native internal fields, with JS-side caches in private fields,
// so that reflection APIs such as Object.getOwnPropertySymbols() and
// Reflect.ownKeys() cannot enumerate them, even after the public getters
// have been invoked and their per-instance caches populated.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { subtle } = globalThis.crypto;

(async () => {
  const keys = [
    await subtle.generateKey(
      { name: 'HMAC', hash: 'SHA-256' }, true, ['sign', 'verify']),
    (await subtle.generateKey(
      { name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign', 'verify'])).privateKey,
    (await subtle.generateKey(
      { name: 'RSA-PSS', modulusLength: 2048,
        publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256' },
      true, ['sign', 'verify'])).publicKey,
    await subtle.generateKey(
      { name: 'AES-GCM', length: 128 }, true, ['encrypt', 'decrypt']),
  ];

  for (const key of keys) {
    // Touch every public getter so any lazy per-instance caching would
    // materialise now.
    /* eslint-disable no-unused-expressions */
    key.type;
    key.extractable;
    key.algorithm;
    key.usages;
    // Read the getters a second time to exercise the cache-hit path.
    key.algorithm;
    key.usages;
    /* eslint-enable no-unused-expressions */

    assert.deepStrictEqual(
      Object.getOwnPropertySymbols(key), [],
      `CryptoKey has own Symbol properties: ${
        Object.getOwnPropertySymbols(key).map(String).join(', ')}`,
    );
    assert.deepStrictEqual(Object.getOwnPropertyNames(key), []);
    assert.deepStrictEqual(Reflect.ownKeys(key), []);
  }
})().then(common.mustCall());
