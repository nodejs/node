'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

const AsyncFunction = async function() {}.constructor;

const methods = [
  'decrypt',
  'decapsulateBits',
  'decapsulateKey',
  'deriveBits',
  'deriveKey',
  'digest',
  'encapsulateBits',
  'encapsulateKey',
  'encrypt',
  'exportKey',
  'generateKey',
  'getPublicKey',
  'importKey',
  'sign',
  'unwrapKey',
  'verify',
  'wrapKey',
];

(async function() {
  for (const name of methods) {
    assert.notStrictEqual(subtle[name].constructor, AsyncFunction);

    const promise = subtle[name].call({});
    assert.strictEqual(Object.getPrototypeOf(promise), Promise.prototype);
    await assert.rejects(promise, {
      code: 'ERR_INVALID_THIS',
    });
  }
})().then(common.mustCall());
