'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { getFips } = require('crypto');
const { subtle } = globalThis.crypto;

(async () => {
  const exponents = getFips() ? [[1, 0, 1]] : [[3], [1, 0, 1]];
  for (const name of ['RSA-PSS', 'RSASSA-PKCS1-v1_5', 'RSA-OAEP']) {
    const usages = name === 'RSA-OAEP' ? ['encrypt', 'decrypt'] : ['sign', 'verify'];
    for (const exponent of exponents) {
      const publicExponent = new Uint8Array([0, 0, 0, 0, ...exponent]);
      const pair = await subtle.generateKey({
        name, modulusLength: 2048, publicExponent, hash: 'SHA-256',
      }, true, usages);
      for (const key of [pair.publicKey, pair.privateKey]) {
        assert.deepStrictEqual(key.algorithm.publicExponent, new Uint8Array(exponent));
      }
      assert.deepStrictEqual(publicExponent, new Uint8Array([0, 0, 0, 0, ...exponent]));
    }
  }
})().then(common.mustCall());
