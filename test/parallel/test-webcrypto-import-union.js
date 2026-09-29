'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;
const jwk = { kty: 'oct', k: 'AAAAAAAAAAAAAAAAAAAAAA' };

(async () => {
  for (const data of [new ArrayBuffer(16), new Uint8Array(16),
                      new DataView(new ArrayBuffer(16)), Buffer.alloc(16)]) {
    await assert.rejects(subtle.importKey('jwk', data, 'AES-GCM', true, ['encrypt']),
                         TypeError);
    Object.assign(data, jwk);
    await assert.rejects(subtle.importKey('jwk', data, 'AES-GCM', true, ['encrypt']),
                         TypeError);
    await assert.rejects(subtle.importKey('jwk', data, 'unknown', true, ['encrypt']),
                         { name: 'NotSupportedError' });
  }
  await assert.rejects(subtle.importKey('raw', {}, 'unknown', true, ['encrypt']),
                       { name: 'NotSupportedError' });
  await assert.rejects(subtle.importKey('raw', {}, 'AES-GCM', true, ['encrypt']),
                       TypeError);
  for (const data of [null, new SharedArrayBuffer(16)]) {
    await assert.rejects(subtle.importKey('jwk', data, 'AES-GCM', true, ['encrypt']),
                         { name: 'DataError' });
  }
  assert.strictEqual((await subtle.importKey('jwk', jwk, 'AES-GCM', true,
                                             ['encrypt'])).type, 'secret');
  await assert.rejects(subtle.importKey('raw', {
    get kty() { throw new Error('converted JWK'); },
  }, 'unknown', true, ['encrypt']), { message: 'converted JWK' });
})().then(common.mustCall());
