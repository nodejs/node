'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createSecretKey } = require('crypto');
const { subtle } = globalThis.crypto;

function check(key) {
  assert.strictEqual(Object.getPrototypeOf(key), CryptoKey.prototype);
  assert.strictEqual(Object.getPrototypeOf(structuredClone(key)), CryptoKey.prototype);
  assert(key instanceof CryptoKey);
}

(async () => {
  check(await subtle.generateKey({ name: 'AES-GCM', length: 128 }, true, ['encrypt']));
  check(await subtle.importKey('raw', new Uint8Array(16), 'AES-GCM', true, ['encrypt']));
  const pair = await subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-256' },
                                        true, ['sign', 'verify']);
  check(pair.publicKey);
  check(pair.privateKey);
  check(createSecretKey(new Uint8Array(16)).toCryptoKey('AES-GCM', true, ['encrypt']));
})().then(common.mustCall());
