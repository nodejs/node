// Flags: --experimental-ffi
'use strict';
const common = require('../common');
common.skipIfFFIMissing();

const assert = require('node:assert');
const { test } = require('node:test');
const ffi = require('node:ffi');
const { libraryPath } = require('./ffi-test-common');

test('null pointer takes fast path (no throw)', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    return_pointer_arg: { result: 'pointer', parameters: ['pointer'] },
  });
  try {
    // Fast path: BigInt
    assert.strictEqual(functions.return_pointer_arg(0xDEADBEEFn), 0xDEADBEEFn);
    // Fast path: null
    assert.strictEqual(functions.return_pointer_arg(null), 0n);
    // Slow path: Buffer (routes via slowInvoke)
    const buf = Buffer.from([1, 2, 3, 4]);
    const ptr = functions.return_pointer_arg(buf);
    assert.strictEqual(typeof ptr, 'bigint');
    assert.notStrictEqual(ptr, 0n);
  } finally {
    lib.close();
  }
});

test('ArrayBuffer routes through slow path', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    return_pointer_arg: { result: 'pointer', parameters: ['pointer'] },
  });
  try {
    const ab = new ArrayBuffer(8);
    const ptr = functions.return_pointer_arg(ab);
    assert.strictEqual(typeof ptr, 'bigint');
    assert.notStrictEqual(ptr, 0n);
  } finally { lib.close(); }
});

test('ArrayBufferView with byteOffset routes through slow path', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    return_pointer_arg: { result: 'pointer', parameters: ['pointer'] },
  });
  try {
    const ab = new ArrayBuffer(16);
    const view = new Uint8Array(ab, 4, 8);  // non-zero byteOffset
    const ptr = functions.return_pointer_arg(view);
    assert.strictEqual(typeof ptr, 'bigint');
    // Pointer should be ab.base + 4, but we don't know the base.
    // Just verify it's non-zero and a bigint.
    assert.notStrictEqual(ptr, 0n);
  } finally { lib.close(); }
});

test('ArrayBufferView with byteOffset propagates the offset', () => {
  // Use return_pointer_arg to echo back the pointer. Call it with two views
  // into the same ArrayBuffer at different offsets; the returned pointers
  // must differ by exactly the offset delta (4 bytes).
  const { lib, functions } = ffi.dlopen(libraryPath, {
    return_pointer_arg: { result: 'pointer', parameters: ['pointer'] },
  });
  try {
    const ab = new ArrayBuffer(32);
    const view0 = new Uint8Array(ab, 0, 16);
    const view4 = new Uint8Array(ab, 4, 16);
    const ptr0 = functions.return_pointer_arg(view0);
    const ptr4 = functions.return_pointer_arg(view4);
    assert.strictEqual(typeof ptr0, 'bigint');
    assert.strictEqual(typeof ptr4, 'bigint');
    assert.strictEqual(ptr4 - ptr0, 4n,
      `expected ptr4 - ptr0 === 4n, got ${ptr4 - ptr0}`);
  } finally { lib.close(); }
});

test('string routes through slow path with declared "string" type', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    return_pointer_arg: { result: 'pointer', parameters: ['string'] },
  });
  try {
    const ptr = functions.return_pointer_arg('hello');
    assert.strictEqual(typeof ptr, 'bigint');
    assert.notStrictEqual(ptr, 0n);
  } finally { lib.close(); }
});

test('undefined pointer maps to 0n', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    return_pointer_arg: { result: 'pointer', parameters: ['pointer'] },
  });
  try {
    assert.strictEqual(functions.return_pointer_arg(undefined), 0n);
  } finally { lib.close(); }
});

test('out-of-range BigInt rejected at wrapper', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    return_pointer_arg: { result: 'pointer', parameters: ['pointer'] },
  });
  try {
    assert.throws(() => functions.return_pointer_arg(-1n),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally { lib.close(); }
});
