'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

(async () => {
  const keyToWrap = await subtle.importKey(
    'raw', new Uint8Array(16), 'AES-GCM', true, ['encrypt']);
  const shortKey = await subtle.importKey(
    'raw', new Uint8Array(8), { name: 'HMAC', hash: 'SHA-256' }, true, ['sign']);

  for (const length of [128, 192, 256]) {
    const wrappingKey = await subtle.generateKey(
      { name: 'AES-KW', length }, false, ['wrapKey', 'unwrapKey']);

    for (const byteLength of [0, 8, 16, 23]) {
      // HKDF accepts an empty key, so the unwrap operation must reject
      // before attempting to import the plaintext as a key.
      await assert.rejects(subtle.unwrapKey(
        'raw', new Uint8Array(byteLength), wrappingKey, 'AES-KW',
        'HKDF', false, ['deriveBits']), { name: 'OperationError' });
    }

    await assert.rejects(subtle.wrapKey(
      'raw', shortKey, wrappingKey, 'AES-KW'), { name: 'OperationError' });

    const wrapped = await subtle.wrapKey(
      'raw', keyToWrap, wrappingKey, 'AES-KW');
    assert.strictEqual(wrapped.byteLength, 24);
    const unwrapped = await subtle.unwrapKey(
      'raw', wrapped, wrappingKey, 'AES-KW', 'AES-GCM', true, ['encrypt']);
    assert.deepStrictEqual(
      new Uint8Array(await subtle.exportKey('raw', unwrapped)),
      new Uint8Array(16));
  }
})().then(common.mustCall());
