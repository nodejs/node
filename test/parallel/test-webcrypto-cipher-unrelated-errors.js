'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { scrypt, scryptSync } = require('crypto');
const { subtle } = globalThis.crypto;

if (typeof scryptSync !== 'function')
  common.skip('no scrypt support');

function failScrypt(sync) {
  // The failed parameter check leaves an error on the main thread's
  // OpenSSL error queue. Cipher jobs must only use their own errors.
  assert.throws(() => {
    if (sync) {
      scryptSync('password', 'salt', 64, { N: 2 ** 17 });
    } else {
      scrypt('password', 'salt', 64, { N: 2 ** 17 }, common.mustNotCall());
    }
  }, { code: 'ERR_CRYPTO_INVALID_SCRYPT_PARAMS' });
}

(async () => {
  const key = await subtle.importKey(
    'raw', new Uint8Array(16), 'AES-GCM', false, ['encrypt', 'decrypt']);
  const algorithm = { name: 'AES-GCM', iv: new Uint8Array(12) };

  for (const sync of [true, false]) {
    for (const byteLength of [0, 16]) {
      const plaintext = new Uint8Array(byteLength);
      failScrypt(sync);
      const ciphertext = await subtle.encrypt(algorithm, key, plaintext);
      assert.strictEqual(ciphertext.byteLength, byteLength + 16);

      failScrypt(sync);
      assert.deepStrictEqual(
        new Uint8Array(await subtle.decrypt(algorithm, key, ciphertext)),
        plaintext);
    }
  }
})().then(common.mustCall());
