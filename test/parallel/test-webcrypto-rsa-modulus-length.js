'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { generateKeyPairSync, KeyObject } = require('crypto');
const { subtle } = globalThis.crypto;

(async () => {
  for (const name of ['RSA-PSS', 'RSASSA-PKCS1-v1_5', 'RSA-OAEP']) {
    const usages = name === 'RSA-OAEP' ? ['encrypt', 'decrypt'] : ['sign', 'verify'];
    for (const modulusLength of [1025, 2048, 2049]) {
      let pair;
      try {
        pair = await subtle.generateKey({
          name, modulusLength, hash: 'SHA-256', publicExponent: new Uint8Array([1, 0, 1]),
        }, true, usages);
      } catch (err) {
        if (modulusLength === 2048 || err.name !== 'OperationError')
          throw err;
        // Only reject sizes the backend also rejects. Some backends round
        // the requested size, which must still produce a usable CryptoKey.
        assert.throws(() => generateKeyPairSync('rsa', { modulusLength }),
                      { name: 'Error' });
        continue;
      }
      const actualModulusLength =
        KeyObject.from(pair.publicKey).asymmetricKeyDetails.modulusLength;
      for (const key of [pair.publicKey, pair.privateKey]) {
        assert.strictEqual(key.algorithm.modulusLength, actualModulusLength);
        assert.strictEqual(
          KeyObject.from(key).asymmetricKeyDetails.modulusLength, actualModulusLength);
        assert.deepStrictEqual(structuredClone(key).algorithm, key.algorithm);

        const format = key.type === 'public' ? 'spki' : 'pkcs8';
        const imported = await subtle.importKey(
          format, await subtle.exportKey(format, key),
          { name, hash: 'SHA-256' }, true, key.usages);
        assert.deepStrictEqual(imported.algorithm, key.algorithm);
      }
      const publicKey = await subtle.getPublicKey(pair.privateKey, pair.publicKey.usages);
      assert.deepStrictEqual(publicKey.algorithm, pair.publicKey.algorithm);
    }
  }
})().then(common.mustCall());
