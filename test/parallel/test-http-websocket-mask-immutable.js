// Flags: --js-immutable-arraybuffer --allow-natives-syntax
'use strict';
const common = require('../common');
const assert = require('assert');
const { websocketMask, websocketUnmask } = require('http');

// transferToImmutable is gated behind --js-immutable-arraybuffer (set above).
// Skip if this V8 build does not expose the API even with the flag set.
if (typeof ArrayBuffer.prototype.transferToImmutable !== 'function')
  common.skip('ArrayBuffer.prototype.transferToImmutable is not available');

const key = Buffer.from([1, 2, 3, 4]);

function immutable(bytes) {
  const ab = new ArrayBuffer(bytes.length);
  new Uint8Array(ab).set(bytes);
  return ab.transferToImmutable();
}

// Masking *into* a view of an immutable ArrayBuffer throws and does not write
// to the read-only backing store, whatever the view type, offset or length.
{
  const bytes = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
  const ab = immutable(bytes);
  const targets = [
    Buffer.from(ab),
    new Uint8Array(ab),
    new Uint32Array(ab),
    new DataView(ab),
    Buffer.from(ab, 4, 8),
  ];
  for (const target of targets) {
    const expectedError = {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
    };
    assert.throws(() => websocketMask(Buffer.alloc(target.byteLength, 9), key, target),
                  { ...expectedError, message: /'output' must not be backed by an immutable ArrayBuffer/ });
    assert.throws(() => websocketMask(Buffer.alloc(4, 9), key, target, 2, 2),
                  expectedError);
    // Zero-length writes are rejected too, as with TypedArray.prototype.fill().
    assert.throws(() => websocketMask(Buffer.alloc(0), key, target), expectedError);
    assert.throws(() => websocketUnmask(target, key),
                  { ...expectedError, message: /'buffer' must not be backed by an immutable ArrayBuffer/ });
    assert.deepStrictEqual([...new Uint8Array(ab)], bytes);
  }

  // Masking a view in place onto itself is a write, so it throws too.
  const view = Buffer.from(ab);
  assert.throws(() => websocketMask(view, key, view), { code: 'ERR_INVALID_ARG_VALUE' });
  assert.deepStrictEqual([...new Uint8Array(ab)], bytes);
}

// Masking *from* a view of an immutable ArrayBuffer is allowed, since reads
// do not require a writable backing store.
{
  const source = Buffer.from(immutable([10, 20, 30, 40, 50, 60, 70, 80]));
  const output = Buffer.alloc(8);
  websocketMask(source, key, output);
  assert.deepStrictEqual([...output], [11, 22, 29, 44, 51, 62, 69, 84]);
  assert.deepStrictEqual([...source], [10, 20, 30, 40, 50, 60, 70, 80]);
}

// The mask itself may be backed by an immutable ArrayBuffer.
{
  const immutableKey = new Uint8Array(immutable([1, 2, 3, 4]));
  const buf = Buffer.from([10, 20, 30, 40]);
  websocketUnmask(buf, immutableKey);
  assert.deepStrictEqual([...buf], [11, 22, 29, 44]);
}

// The optimized (fast API) path also rejects immutable targets.
{
  const target = Buffer.from(immutable([1, 2, 3, 4, 5, 6, 7, 8]));
  const source = Buffer.alloc(8, 9);
  function tryMask(output) {
    try {
      websocketMask(source, key, output);
      return true;
    } catch (err) {
      assert.strictEqual(err.code, 'ERR_INVALID_ARG_VALUE');
      return false;
    }
  }
  eval('%PrepareFunctionForOptimization(websocketMask)');
  assert.strictEqual(tryMask(Buffer.alloc(8)), true);
  eval('%OptimizeFunctionOnNextCall(websocketMask)');
  assert.strictEqual(tryMask(target), false);
  assert.deepStrictEqual([...target], [1, 2, 3, 4, 5, 6, 7, 8]);
}
