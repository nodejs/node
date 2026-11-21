'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

(async () => {
  for (const name of ['MLKEM768-P256', 'MLKEM768-X25519', 'MLKEM1024-P384']) {
    if (!SubtleCrypto.supports('generateKey', name)) continue;
    const { privateKey, publicKey } = await subtle.generateKey(
      name, true, ['encapsulateBits', 'decapsulateBits']);
    const seed = await subtle.exportKey('raw-seed', privateKey);
    const publicBytes = await subtle.exportKey('raw-public', publicKey);
    const descriptor = Object.getOwnPropertyDescriptor(Uint8Array, Symbol.species);
    try {
      Object.defineProperty(Uint8Array, Symbol.species, {
        configurable: true, get: common.mustNotCall(),
      });
      const imported = await subtle.importKey('raw-seed', seed, name, true,
                                              ['decapsulateBits']);
      assert.deepStrictEqual(await subtle.exportKey('raw-seed', imported), seed);
      const publicImported = await subtle.importKey('raw-public', publicBytes, name,
                                                    true, ['encapsulateBits']);
      assert.deepStrictEqual(await subtle.exportKey('raw-public', publicImported), publicBytes);
    } finally {
      if (descriptor) {
        Object.defineProperty(Uint8Array, Symbol.species, descriptor);
      } else {
        delete Uint8Array[Symbol.species];
      }
    }
  }
})().then(common.mustCall());
