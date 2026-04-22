// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');

const { converters } = require('internal/webidl');

const TYPED_ARRAY_CTORS = [
  Uint8Array, Int8Array, Uint8ClampedArray,
  Uint16Array, Int16Array,
  Uint32Array, Int32Array,
  Float16Array, Float32Array, Float64Array,
  BigInt64Array, BigUint64Array,
];

test('BufferSource accepts ArrayBuffer', () => {
  const ab = new ArrayBuffer(8);
  assert.strictEqual(converters.BufferSource(ab), ab);
});

test('BufferSource accepts all TypedArray kinds', () => {
  for (const Ctor of TYPED_ARRAY_CTORS) {
    const ta = new Ctor(4);
    assert.strictEqual(converters.BufferSource(ta), ta);
  }
});

test('BufferSource accepts Buffer', () => {
  const buf = Buffer.alloc(8);
  assert.strictEqual(converters.BufferSource(buf), buf);
});

test('BufferSource accepts DataView', () => {
  const dv = new DataView(new ArrayBuffer(8));
  assert.strictEqual(converters.BufferSource(dv), dv);
});

test('BufferSource accepts ArrayBuffer subclass instance', () => {
  class MyAB extends ArrayBuffer {}
  const sub = new MyAB(8);
  assert.strictEqual(converters.BufferSource(sub), sub);
});

test('BufferSource accepts TypedArray with null prototype', () => {
  const ta = new Uint8Array(4);
  Object.setPrototypeOf(ta, null);
  assert.strictEqual(converters.BufferSource(ta), ta);
});

test('BufferSource accepts DataView with null prototype', () => {
  const dv = new DataView(new ArrayBuffer(4));
  Object.setPrototypeOf(dv, null);
  assert.strictEqual(converters.BufferSource(dv), dv);
});

test('BufferSource accepts ArrayBuffer with null prototype', () => {
  const ab = new ArrayBuffer(4);
  Object.setPrototypeOf(ab, null);
  assert.strictEqual(converters.BufferSource(ab), ab);
});

test('BufferSource rejects SharedArrayBuffer', () => {
  assert.throws(
    () => converters.BufferSource(new SharedArrayBuffer(4)),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('BufferSource rejects SAB-backed TypedArray', () => {
  const view = new Uint8Array(new SharedArrayBuffer(4));
  assert.throws(
    () => converters.BufferSource(view),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('BufferSource rejects SAB-backed DataView', () => {
  const dv = new DataView(new SharedArrayBuffer(4));
  assert.throws(
    () => converters.BufferSource(dv),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('BufferSource rejects SAB view whose buffer prototype was reassigned', () => {
  const sab = new SharedArrayBuffer(4);
  Object.setPrototypeOf(sab, ArrayBuffer.prototype);
  const view = new Uint8Array(sab);
  assert.throws(
    () => converters.BufferSource(view),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('BufferSource accepts a detached ArrayBuffer', () => {
  const ab = new ArrayBuffer(8);
  structuredClone(ab, { transfer: [ab] });
  assert.strictEqual(ab.byteLength, 0);
  assert.strictEqual(converters.BufferSource(ab), ab);
});

test('BufferSource rejects objects with a forged @@toStringTag', () => {
  const fake = { [Symbol.toStringTag]: 'Uint8Array' };
  assert.throws(
    () => converters.BufferSource(fake),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

for (const value of [null, undefined, 0, 1, 1n, '', 'x', true, Symbol('s'), [],
                     {}, () => {}]) {
  test(`BufferSource rejects ${typeof value} ${String(value)}`, () => {
    assert.throws(
      () => converters.BufferSource(value),
      { code: 'ERR_INVALID_ARG_TYPE' },
    );
  });
}

test('ArrayBufferView accepts all TypedArray kinds', () => {
  for (const Ctor of TYPED_ARRAY_CTORS) {
    const ta = new Ctor(4);
    assert.strictEqual(converters.ArrayBufferView(ta), ta);
  }
});

test('ArrayBufferView accepts DataView', () => {
  const dv = new DataView(new ArrayBuffer(8));
  assert.strictEqual(converters.ArrayBufferView(dv), dv);
});

test('ArrayBufferView accepts TypedArray subclass instance', () => {
  class MyU8 extends Uint8Array {}
  const sub = new MyU8(4);
  assert.strictEqual(converters.ArrayBufferView(sub), sub);
});

test('ArrayBufferView accepts TypedArray with null prototype', () => {
  const ta = new Uint8Array(4);
  Object.setPrototypeOf(ta, null);
  assert.strictEqual(converters.ArrayBufferView(ta), ta);
});

test('ArrayBufferView accepts DataView with null prototype', () => {
  const dv = new DataView(new ArrayBuffer(4));
  Object.setPrototypeOf(dv, null);
  assert.strictEqual(converters.ArrayBufferView(dv), dv);
});

test('ArrayBufferView rejects raw ArrayBuffer', () => {
  assert.throws(
    () => converters.ArrayBufferView(new ArrayBuffer(4)),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('ArrayBufferView rejects raw SharedArrayBuffer', () => {
  assert.throws(
    () => converters.ArrayBufferView(new SharedArrayBuffer(4)),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('ArrayBufferView rejects SAB-backed TypedArray', () => {
  const view = new Uint8Array(new SharedArrayBuffer(4));
  assert.throws(
    () => converters.ArrayBufferView(view),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('ArrayBufferView rejects SAB-backed DataView', () => {
  const dv = new DataView(new SharedArrayBuffer(4));
  assert.throws(
    () => converters.ArrayBufferView(dv),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('ArrayBufferView rejects SAB view whose buffer prototype was reassigned', () => {
  const sab = new SharedArrayBuffer(4);
  Object.setPrototypeOf(sab, ArrayBuffer.prototype);
  const view = new Uint8Array(sab);
  assert.throws(
    () => converters.ArrayBufferView(view),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('ArrayBufferView rejects objects with a forged @@toStringTag', () => {
  const fake = { [Symbol.toStringTag]: 'Uint8Array' };
  assert.throws(
    () => converters.ArrayBufferView(fake),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

for (const value of [null, undefined, 0, 1, 1n, '', 'x', true, Symbol('s'), [],
                     {}, () => {}]) {
  test(`ArrayBufferView rejects ${typeof value} ${String(value)}`, () => {
    assert.throws(
      () => converters.ArrayBufferView(value),
      { code: 'ERR_INVALID_ARG_TYPE' },
    );
  });
}
