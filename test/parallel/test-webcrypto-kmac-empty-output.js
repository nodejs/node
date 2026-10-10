'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasFIPS, isBoringSSL } = require('../common/crypto');

if (isBoringSSL)
  common.skip('KMAC is not supported by BoringSSL');
if (hasFIPS())
  common.skip('empty KMAC output is not supported in FIPS mode');

const assert = require('assert');
const { subtle } = globalThis.crypto;

(async () => {
  const data = new Uint8Array([1, 2, 3]);

  for (const name of ['KMAC128', 'KMAC256']) {
    const key = await subtle.importKey(
      'raw-secret', new Uint8Array(32), name, false, ['sign', 'verify']);
    const otherKey = await subtle.importKey(
      'raw-secret', new Uint8Array(32).fill(1), name, false, ['verify']);
    const algorithm = { name, outputLength: 0 };
    const signature = await subtle.sign(algorithm, key, data);

    assert.strictEqual(signature.byteLength, 0);
    assert.strictEqual(await subtle.verify(algorithm, key, signature, data), true);
    assert.strictEqual(await subtle.verify(algorithm, key, new Uint8Array([0]), data), false);
    assert.strictEqual(await subtle.verify(
      { name, outputLength: 256 }, key, signature, data), false);

    // Empty MACs compare equal even when the inputs differ.
    assert.strictEqual(await subtle.verify(
      algorithm, key, signature, new Uint8Array([4, 5, 6])), true);
    assert.strictEqual(await subtle.verify(algorithm, otherKey, signature, data), true);
    assert.strictEqual(await subtle.verify(
      { ...algorithm, customization: new Uint8Array([1]) }, key, signature, data), true);
  }
})().then(common.mustCall());
