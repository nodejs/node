'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

(async () => {
  const key = await subtle.importKey('raw', new Uint8Array(16), 'AES-GCM', true,
                                     ['encrypt', 'decrypt']);
  const descriptor = Object.getOwnPropertyDescriptor(Array.prototype, 'constructor');
  let usages;
  let jwk;
  try {
    Object.defineProperty(Array.prototype, 'constructor', { get: common.mustNotCall() });
    usages = key.usages;
    jwk = await subtle.exportKey('jwk', key);
  } finally {
    Object.defineProperty(Array.prototype, 'constructor', descriptor);
  }
  assert.strictEqual(Object.getPrototypeOf(usages), Array.prototype);
  assert.strictEqual(key.usages, usages);
  assert.deepStrictEqual(usages, ['encrypt', 'decrypt']);
  assert.deepStrictEqual(jwk.key_ops, usages);
  usages.push('sign');
  assert.deepStrictEqual((await subtle.exportKey('jwk', key)).key_ops,
                         ['encrypt', 'decrypt']);
})().then(common.mustCall());
