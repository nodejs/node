'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createCipheriv } = require('crypto');
const { subtle } = globalThis.crypto;

(async () => {
  const plaintext = Buffer.from('AES-GCM with a variable-length IV');
  const additionalData = Buffer.from('additional data');

  for (const length of [128, 192, 256]) {
    const keyBytes = Buffer.alloc(length / 8);
    const key = await subtle.importKey(
      'raw', keyBytes, 'AES-GCM', false, ['encrypt', 'decrypt']);

    for (const ivLength of [1, 8, 11, 12, 16, 128]) {
      const iv = Buffer.alloc(ivLength, 1);
      const algorithm = { name: 'AES-GCM', iv, additionalData };
      const cipher = createCipheriv(`aes-${length}-gcm`, keyBytes, iv);
      cipher.setAAD(additionalData);
      const expected = Buffer.concat([
        cipher.update(plaintext), cipher.final(), cipher.getAuthTag(),
      ]);

      assert.deepStrictEqual(
        Buffer.from(await subtle.encrypt(algorithm, key, plaintext)), expected);
      assert.deepStrictEqual(
        Buffer.from(await subtle.decrypt(algorithm, key, expected)), plaintext);
    }

    const algorithm = { name: 'AES-GCM', iv: new Uint8Array(0) };
    await assert.rejects(subtle.encrypt(algorithm, key, plaintext),
                         { name: 'OperationError' });
    await assert.rejects(subtle.decrypt(algorithm, key, new Uint8Array(16)),
                         { name: 'OperationError' });
  }
})().then(common.mustCall());
