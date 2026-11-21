'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { supports } = SubtleCrypto;

for (const operation of ['wrapKey', 'unwrapKey']) {
  for (const length of [undefined, null, 0, 128]) {
    assert.strictEqual(supports(operation, 'AES-KW', length), true);
  }
  assert.strictEqual(supports(operation, 'AES-KW'), true);
}

for (const operation of ['digest', 'unknown', 'wrapKey', 'deriveBits']) {
  for (const value of [-1, NaN, Infinity, 2 ** 32, Symbol()]) {
    assert.throws(() => supports(operation, 'SHA-256', value), TypeError);
  }
}

const hkdf = {
  name: 'HKDF', hash: 'SHA-256', salt: new Uint8Array(), info: new Uint8Array(),
};
assert.strictEqual(supports('deriveBits', hkdf, 128), true);
for (const value of ['128', 'AES-GCM', {}, true, 128n]) {
  assert.strictEqual(supports('deriveBits', hkdf, value), false);
}
assert.strictEqual(supports('deriveKey', hkdf), false);
assert.strictEqual(supports('deriveKey', hkdf, { name: 'AES-GCM', length: 128 }), true);

if (supports('encapsulateBits', 'ML-KEM-768')) {
  assert.strictEqual(supports('encapsulateKey', 'ML-KEM-768'), true);
  assert.strictEqual(supports('decapsulateKey', 'ML-KEM-768', 128), true);
}
