'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const webcrypto = globalThis.crypto;

for (const Type of [Uint8Array, Uint16Array, Uint32Array, BigUint64Array]) {
  const storage = new Uint8Array(128);
  const view = new Type(storage.buffer, 32, 32 / Type.BYTES_PER_ELEMENT);
  for (const name of ['buffer', 'byteOffset', 'byteLength', 'BYTES_PER_ELEMENT']) {
    Object.defineProperty(view, name, { get: common.mustNotCall() });
    Object.defineProperty(storage.buffer, name, { get: common.mustNotCall() });
  }
  assert.strictEqual(webcrypto.getRandomValues(view), view);
  assert(storage.subarray(32, 64).some((byte) => byte !== 0));
  assert(storage.subarray(0, 32).every((byte) => byte === 0));
  assert(storage.subarray(64).every((byte) => byte === 0));
}

const oversized = new Uint8Array(65537);
Object.defineProperty(oversized, 'byteLength', { value: 0 });
assert.throws(() => webcrypto.getRandomValues(oversized), {
  name: 'QuotaExceededError',
});

const empty = new Uint8Array(0);
Object.defineProperty(empty, 'byteLength', { value: 65537 });
assert.strictEqual(webcrypto.getRandomValues(empty), empty);
