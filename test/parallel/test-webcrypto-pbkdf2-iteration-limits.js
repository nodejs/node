'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

(async () => {
  const key = await subtle.importKey('raw', new Uint8Array(16), 'PBKDF2', false, ['deriveBits']);
  for (const iterations of [2 ** 31, 2 ** 32 - 1]) {
    const algorithm = { name: 'PBKDF2', hash: 'SHA-256', salt: new Uint8Array(16), iterations };
    assert.strictEqual((await subtle.deriveBits(algorithm, key, 0)).byteLength, 0);
    assert.strictEqual(SubtleCrypto.supports('deriveBits', algorithm, 0), true);
    assert.strictEqual(SubtleCrypto.supports('deriveBits', algorithm, 8), false);
    await assert.rejects(subtle.deriveBits(algorithm, key, 8), { name: 'OperationError' });
  }
  const algorithm = { name: 'PBKDF2', hash: 'SHA-256', salt: new Uint8Array(16), iterations: 0 };
  await assert.rejects(subtle.deriveBits(algorithm, key, 0), { name: 'OperationError' });
})().then(common.mustCall());
