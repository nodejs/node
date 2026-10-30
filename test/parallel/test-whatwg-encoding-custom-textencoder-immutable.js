// Flags: --js-immutable-arraybuffer
'use strict';
const common = require('../common');
const assert = require('assert');

if (typeof ArrayBuffer.prototype.transferToImmutable !== 'function')
  common.skip('ArrayBuffer.prototype.transferToImmutable is not available');

const encoder = new TextEncoder();
const original = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];

function immutable() {
  const ab = new ArrayBuffer(original.length);
  new Uint8Array(ab).set(original);
  return ab.transferToImmutable();
}

// Strings longer than 32 code units take the fast path in the binding, so
// both write paths are exercised.
const inputs = [
  '',
  'hi',
  'héllo wörld',
  'a'.repeat(64),
  '日本語のテキスト'.repeat(8),
  '\uD800 lone surrogate',
];

const views = [
  (ab) => new Uint8Array(ab),
  (ab) => new Uint8Array(ab, 4, 8),
  (ab) => new Uint8Array(ab, 0, 0),
  (ab) => new Uint8Array(ab, ab.byteLength, 0),
];

for (const input of inputs) {
  for (const makeView of views) {
    const ab = immutable();
    const dest = makeView(ab);
    assert.throws(() => encoder.encodeInto(input, dest), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: /'dest' must not be backed by an immutable ArrayBuffer/,
    });
    assert.deepStrictEqual([...new Uint8Array(ab)], original);
  }
}

// Mutable buffers are unaffected.
{
  const dest = new Uint8Array(4);
  assert.deepStrictEqual(encoder.encodeInto('hi', dest), { read: 2, written: 2 });
  assert.deepStrictEqual([...dest], [104, 105, 0, 0]);
}
