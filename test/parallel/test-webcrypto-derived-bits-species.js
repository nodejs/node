'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

(async () => {
  const keys = await subtle.generateKey({ name: 'ECDH', namedCurve: 'P-256' }, false, ['deriveBits']);
  const algorithm = { name: 'ECDH', public: keys.publicKey };
  const expected = await subtle.deriveBits(algorithm, keys.privateKey, 128);
  const species = Object.getOwnPropertyDescriptor(ArrayBuffer, Symbol.species);
  Object.defineProperty(ArrayBuffer, Symbol.species, {
    configurable: true,
    get: common.mustNotCall(),
  });
  try {
    const actual = await subtle.deriveBits(algorithm, keys.privateKey, 128);
    assert.deepStrictEqual(actual, expected);
    assert.strictEqual(Object.getPrototypeOf(actual), ArrayBuffer.prototype);
  } finally {
    Object.defineProperty(ArrayBuffer, Symbol.species, species);
  }
})().then(common.mustCall());
