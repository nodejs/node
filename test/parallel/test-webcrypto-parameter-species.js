'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

function withSpecies(callback) {
  Object.defineProperty(Uint8Array, Symbol.species, {
    configurable: true,
    value: Float64Array,
  });
  try {
    return callback();
  } finally {
    delete Uint8Array[Symbol.species];
  }
}

(async () => {
  const key = await subtle.importKey('raw', new Uint8Array(16),
                                     'AES-GCM', false, ['encrypt']);
  const algorithm = { name: 'AES-GCM', iv: new Uint8Array(12).fill(1) };
  const data = new Uint8Array(16);
  const expected = await subtle.encrypt(algorithm, key, data);
  assert.deepStrictEqual(await withSpecies(() => subtle.encrypt(algorithm, key, data)), expected);

  const hmac = { name: 'HMAC', hash: 'SHA-256', length: 100 };
  const secret = new Uint8Array(13).fill(1);
  const normal = await subtle.importKey('raw', secret, hmac, true, ['sign']);
  const tampered = await withSpecies(() => subtle.importKey('raw', secret, hmac, true, ['sign']));
  assert.deepStrictEqual(await subtle.exportKey('raw', tampered), await subtle.exportKey('raw', normal));
  assert.deepStrictEqual(await subtle.sign('HMAC', tampered, data), await subtle.sign('HMAC', normal, data));
})().then(common.mustCall());
