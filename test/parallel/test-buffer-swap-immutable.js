// Flags: --js-immutable-arraybuffer --allow-natives-syntax --expose-internals --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const binding = internalBinding('buffer');
const { arrayBufferViewHasBuffer } = internalBinding('util');

if (typeof ArrayBuffer.prototype.transferToImmutable !== 'function')
  common.skip('ArrayBuffer.prototype.transferToImmutable is not available');

let debug;
if (common.isDebug) {
  debug = internalBinding('debug');
}

function assertImmutableError(fn, native = false) {
  // Constructing the error must not invoke an inherited code setter.
  let setterCalls = 0;
  Object.defineProperty(Error.prototype, 'code', {
    configurable: true,
    get() { return undefined; },
    set() { setterCalls++; },
  });
  try {
    assert.throws(fn, native ? {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
    } : TypeError);
  } finally {
    delete Error.prototype.code;
  }
  assert.strictEqual(setterCalls, 0);
}

for (const [method, swapped] of [
  ['swap16', [2, 1, 4, 3, 6, 5, 8, 7]],
  ['swap32', [4, 3, 2, 1, 8, 7, 6, 5]],
  ['swap64', [8, 7, 6, 5, 4, 3, 2, 1]],
]) {
  const expected64 = Array.from({ length: 64 },
                                (_, i) => swapped[i % 8] + 8 * Math.floor(i / 8));

  // Exercise both the JavaScript and native paths, including unaligned views.
  for (const length of [8, 64]) {
    const bytes = Array.from({ length }, (_, i) => i + 1);
    for (const offset of [0, 1]) {
      const original = offset === 0 ? bytes : [0xaa, ...bytes, 0xbb];
      const ab = Uint8Array.from(original).buffer.transferToImmutable();
      const target = Buffer.from(ab, offset, length);
      assertImmutableError(() => target[method](), length === 64);
      assert.deepStrictEqual([...new Uint8Array(ab)], original);
    }

    const mutable = Buffer.from([0xaa, ...bytes, 0xbb]);
    const target = mutable.subarray(1, length + 1);
    for (const name of ['buffer', 'byteOffset', 'byteLength']) {
      Object.defineProperty(target, name, { get: common.mustNotCall() });
    }
    assert.strictEqual(target[method](), target);
    assert.deepStrictEqual([...mutable],
                           [0xaa, ...expected64.slice(0, length), 0xbb]);
  }

  // Invalid sizes take precedence over immutable backing stores.
  for (const length of [7, 65]) {
    const bytes = Array.from({ length }, (_, i) => i);
    const ab = Uint8Array.from(bytes).buffer.transferToImmutable();
    assert.throws(() => Buffer.from(ab)[method](), {
      code: 'ERR_INVALID_BUFFER_SIZE',
    });
    assert.deepStrictEqual([...new Uint8Array(ab)], bytes);
  }

  // Empty buffers and empty views do not write to their backing store.
  {
    const empty = Buffer.from(new ArrayBuffer(0).transferToImmutable());
    assert.strictEqual(empty[method](), empty);
    const ab = Uint8Array.from([1, 2, 3]).buffer.transferToImmutable();
    const view = Buffer.from(ab, 1, 0);
    assert.strictEqual(view[method](), view);
    assert.deepStrictEqual([...new Uint8Array(ab)], [1, 2, 3]);
  }

  // The binding returns success, or false without touching immutable bytes.
  {
    const bytes = Array.from({ length: 66 }, (_, i) => i);
    for (const backing of [new ArrayBuffer(66), new SharedArrayBuffer(66)]) {
      new Uint8Array(backing).set(bytes);
      assert.strictEqual(binding[method](backing, 1, 64), true);
      assert.deepStrictEqual([...new Uint8Array(backing)],
                             [0, ...expected64, 65]);
    }
    const backing = Uint8Array.from(bytes).buffer.transferToImmutable();
    assert.strictEqual(binding[method](backing, 1, 64), false);
    assert.deepStrictEqual([...new Uint8Array(backing)], bytes);
  }

  // Shared and resized backing stores retain the view's offset and length.
  for (const backing of [
    new SharedArrayBuffer(66),
    new ArrayBuffer(72, { maxByteLength: 80 }),
  ]) {
    const target = Buffer.from(backing, 1, 64);
    if (backing.resizable) backing.resize(66);
    new Uint8Array(backing).set(Array.from({ length: 66 }, (_, i) => i));
    assert.strictEqual(target[method](), target);
    assert.deepStrictEqual([...new Uint8Array(backing)],
                           [0, ...expected64, 65]);
  }

  // A fresh on-heap TypedArray must materialize its backing ArrayBuffer.
  {
    const target = new Uint8Array(64);
    for (let i = 0; i < target.length; i++) target[i] = i + 1;
    assert.strictEqual(arrayBufferViewHasBuffer(target), false);
    assert.strictEqual(Buffer.prototype[method].call(target), target);
    assert.strictEqual(arrayBufferViewHasBuffer(target), true);
    assert.deepStrictEqual([...target], expected64);
  }

  // The optimized native path must reject immutable targets before writing.
  {
    const bytes = Array.from({ length: 66 }, (_, i) => i);
    const ab = Uint8Array.from(bytes).buffer.transferToImmutable();
    const target = Buffer.from(ab, 1, 64);
    eval('%PrepareFunctionForOptimization(Buffer.prototype[method])');
    Buffer.alloc(64)[method]();
    const fastCallsBefore = debug?.getV8FastApiCallCount(`buffer.${method}`);
    eval('%OptimizeFunctionOnNextCall(Buffer.prototype[method])');
    assertImmutableError(() => target[method](), true);
    if (debug) {
      assert.strictEqual(debug.getV8FastApiCallCount(`buffer.${method}`),
                         fastCallsBefore + 1);
    }
    assert.deepStrictEqual([...new Uint8Array(ab)], bytes);

    // Materializing a fresh receiver must also happen before the fast call.
    const fresh = new Uint8Array(64);
    for (let i = 0; i < fresh.length; i++) fresh[i] = i + 1;
    assert.strictEqual(arrayBufferViewHasBuffer(fresh), false);
    const freshFastCallsBefore = debug?.getV8FastApiCallCount(`buffer.${method}`);
    eval('%OptimizeFunctionOnNextCall(Buffer.prototype[method])');
    assert.strictEqual(Buffer.prototype[method].call(fresh), fresh);
    assert.strictEqual(arrayBufferViewHasBuffer(fresh), true);
    if (debug) {
      assert.strictEqual(debug.getV8FastApiCallCount(`buffer.${method}`),
                         freshFastCallsBefore + 1);
    }
    assert.deepStrictEqual([...fresh], expected64);
  }
}
