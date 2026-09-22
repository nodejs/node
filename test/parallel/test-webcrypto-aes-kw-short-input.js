'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { getFips } = require('crypto');
const { hasOpenSSL } = require('../common/crypto');
const { subtle } = globalThis.crypto;

(async () => {
  const keyToWrap = await subtle.importKey(
    'raw', new Uint8Array(16), 'AES-GCM', true, ['encrypt']);
  let emptyKey;
  if (hasOpenSSL(3) && getFips() !== 1) {
    emptyKey = await subtle.importKey(
      'raw-secret', new Uint8Array(0), 'KMAC128', true, ['sign']);
  }

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

    if (emptyKey !== undefined) {
      await assert.rejects(subtle.wrapKey(
        'raw-secret', emptyKey, wrappingKey, 'AES-KW'),
                           { name: 'OperationError' });
    }

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
