// Flags: --js-immutable-arraybuffer
'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');
if (typeof ArrayBuffer.prototype.transferToImmutable !== 'function')
  common.skip('ArrayBuffer.prototype.transferToImmutable is not available');

const assert = require('assert');
const crypto = require('crypto');

const bytes = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
const expectedError = {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
  message: "The 'buf' argument must not be backed by an immutable ArrayBuffer",
};

function immutable() {
  return Uint8Array.from(bytes).buffer.transferToImmutable();
}

const factories = [
  (ab) => ab,
  (ab) => Buffer.from(ab, 4, 8),
  (ab) => new Uint8Array(ab, 4, 8),
  (ab) => new DataView(ab, 4, 8),
  (ab) => new Uint16Array(ab, 4, 4),
];

for (const create of factories) {
  const ab = immutable();
  const input = create(ab);
  const length = input.length ?? input.byteLength;

  // Reject full and partial fills, including each callback overload, before
  // writing to any part of the backing store.
  for (const args of [[], [1], [1, 1]]) {
    assert.throws(() => crypto.randomFillSync(input, ...args), expectedError);
    assert.throws(() => crypto.randomFill(input, ...args, common.mustNotCall()),
                  expectedError);
  }

  // Zero-byte fills remain no-ops, including an omitted size at the end.
  for (const args of [[0, 0], [length]]) {
    assert.strictEqual(crypto.randomFillSync(input, ...args), input);
    crypto.randomFill(input, ...args, common.mustSucceed((result) => {
      assert.strictEqual(result, input);
    }));
  }
  assert.strictEqual(ab.immutable, true);
  assert.deepStrictEqual([...new Uint8Array(ab)], bytes);
}

// Shared backing stores, including those accessed through views, remain writable.
for (const create of factories) {
  const input = create(new SharedArrayBuffer(16));
  assert.strictEqual(crypto.randomFillSync(input), input);
  crypto.randomFill(input, common.mustSucceed((result) => {
    assert.strictEqual(result, input);
  }));
}
for (const api of [crypto, globalThis.crypto]) {
  const ab = immutable();
  for (const input of [
    Buffer.from(ab, 4, 8),
    new Uint8Array(ab, 4, 8),
    new Uint16Array(ab, 4, 4),
  ]) {
    assert.throws(() => api.getRandomValues(input), expectedError);
  }
  assert.strictEqual(ab.immutable, true);
  assert.deepStrictEqual([...new Uint8Array(ab)], bytes);
}

// Empty immutable buffers and views do not require a writable backing store.
{
  const ab = new ArrayBuffer(0).transferToImmutable();
  for (const input of [ab, Buffer.from(ab), new Uint8Array(ab),
                       new DataView(ab), new Uint16Array(ab)]) {
    assert.strictEqual(crypto.randomFillSync(input), input);
    crypto.randomFill(input, common.mustSucceed((result) => {
      assert.strictEqual(result, input);
    }));
  }
  for (const api of [crypto, globalThis.crypto]) {
    for (const input of [Buffer.from(ab), new Uint8Array(ab)]) {
      assert.strictEqual(api.getRandomValues(input), input);
    }
  }
}

// Inspect the real backing store, regardless of shadowed public properties.
{
  const ab = immutable();
  const input = new Uint8Array(ab, 4, 8);
  Object.defineProperty(ab, 'immutable', { value: false });
  Object.defineProperties(input, {
    buffer: { value: new ArrayBuffer(ab.byteLength) },
    immutable: { value: false },
  });
  assert.throws(() => crypto.randomFillSync(input), expectedError);
  assert.throws(() => crypto.randomFill(input, common.mustNotCall()),
                expectedError);
  assert.deepStrictEqual([...new Uint8Array(ab)], bytes);
}
