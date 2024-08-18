'use strict';

const common = require('../common');
const assert = require('assert');
const { runInNewContext } = require('vm');

const checkString = 'test';

const check = Buffer.from(checkString);

class MyString extends String {
  constructor() {
    super(checkString);
  }
}

class MyPrimitive {
  [Symbol.toPrimitive]() {
    return checkString;
  }
}

class MyBadPrimitive {
  [Symbol.toPrimitive]() {
    return 1;
  }
}

assert.deepStrictEqual(Buffer.from(new String(checkString)), check);
assert.deepStrictEqual(Buffer.from(new MyString()), check);
assert.deepStrictEqual(Buffer.from(new MyPrimitive()), check);
assert.deepStrictEqual(
  Buffer.from(runInNewContext('new String(checkString)', { checkString })),
  check
);

[
  {},
  new Boolean(true),
  { valueOf() { return null; } },
  { valueOf() { return undefined; } },
  { valueOf: null },
  { __proto__: null },
  new Number(true),
  new MyBadPrimitive(),
  Symbol(),
  5n,
  (one, two, three) => {},
  undefined,
  null,
].forEach((input) => {
  assert.throws(() => Buffer.from(input), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => Buffer.from(input, 'hex'), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

Buffer.allocUnsafe(10); // Should not throw.
Buffer.from('deadbeaf', 'hex'); // Should not throw.


{
  const u16 = new Uint16Array([0xffff]);
  const b16 = Buffer.copyBytesFrom(u16);
  u16[0] = 0;
  assert.strictEqual(b16.length, 2);
  assert.strictEqual(b16[0], 255);
  assert.strictEqual(b16[1], 255);
}

{
  const u16 = new Uint16Array([0, 0xffff]);
  const b16 = Buffer.copyBytesFrom(u16, 1, 5);
  u16[0] = 0xffff;
  u16[1] = 0;
  assert.strictEqual(b16.length, 2);
  assert.strictEqual(b16[0], 255);
  assert.strictEqual(b16[1], 255);
}

{
  const u32 = new Uint32Array([0xffffffff]);
  const b32 = Buffer.copyBytesFrom(u32);
  u32[0] = 0;
  assert.strictEqual(b32.length, 4);
  assert.strictEqual(b32[0], 255);
  assert.strictEqual(b32[1], 255);
  assert.strictEqual(b32[2], 255);
  assert.strictEqual(b32[3], 255);
}

assert.throws(() => {
  Buffer.copyBytesFrom();
}, {
  code: 'ERR_INVALID_ARG_TYPE',
});

['', Symbol(), true, false, {}, [], () => {}, 1, 1n, null, undefined].forEach(
  (notTypedArray) => assert.throws(() => {
    Buffer.copyBytesFrom('nope');
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  })
);

['', Symbol(), true, false, {}, [], () => {}, 1n].forEach((notANumber) =>
  assert.throws(() => {
    Buffer.copyBytesFrom(new Uint8Array(1), notANumber);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  })
);

[-1, NaN, 1.1, -Infinity].forEach((outOfRange) =>
  assert.throws(() => {
    Buffer.copyBytesFrom(new Uint8Array(1), outOfRange);
  }, {
    code: 'ERR_OUT_OF_RANGE',
  })
);

['', Symbol(), true, false, {}, [], () => {}, 1n].forEach((notANumber) =>
  assert.throws(() => {
    Buffer.copyBytesFrom(new Uint8Array(1), 0, notANumber);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  })
);

[-1, NaN, 1.1, -Infinity].forEach((outOfRange) =>
  assert.throws(() => {
    Buffer.copyBytesFrom(new Uint8Array(1), 0, outOfRange);
  }, {
    code: 'ERR_OUT_OF_RANGE',
  })
);

// copyBytesFrom: length exceeds view (should clamp, not throw)
{
  const u8 = new Uint8Array([1, 2, 3, 4, 5]);
  const b = Buffer.copyBytesFrom(u8, 2, 100);
  assert.strictEqual(b.length, 3);
  assert.deepStrictEqual([...b], [3, 4, 5]);
}

// copyBytesFrom: length 0 returns empty buffer
{
  const u8 = new Uint8Array([1, 2, 3]);
  const b = Buffer.copyBytesFrom(u8, 1, 0);
  assert.strictEqual(b.length, 0);
}

// copyBytesFrom: offset past end returns empty buffer
{
  const u8 = new Uint8Array([1, 2, 3]);
  const b = Buffer.copyBytesFrom(u8, 10);
  assert.strictEqual(b.length, 0);
}

// copyBytesFrom: Float64Array with offset and length
{
  const f64 = new Float64Array([1.0, 2.0, 3.0, 4.0]);
  const b = Buffer.copyBytesFrom(f64, 1, 2);
  assert.strictEqual(b.length, 16);
  const view = new Float64Array(b.buffer, b.byteOffset, 2);
  assert.strictEqual(view[0], 2.0);
  assert.strictEqual(view[1], 3.0);
}

// copyBytesFrom: empty typed array returns empty buffer
{
  const empty = new Uint8Array(0);
  const b = Buffer.copyBytesFrom(empty);
  assert.strictEqual(b.length, 0);
}

// copyBytesFrom: ArrayBuffer
{
  const ab = new ArrayBuffer(4);
  new Uint8Array(ab).set([1, 2, 3, 4]);
  const b = Buffer.copyBytesFrom(ab);
  new Uint8Array(ab).set([0, 0, 0, 0]);
  assert.strictEqual(b.length, 4);
  assert.deepStrictEqual([...b], [1, 2, 3, 4]);
}

// copyBytesFrom: ArrayBuffer with offset and length
{
  const ab = new ArrayBuffer(4);
  new Uint8Array(ab).set([1, 2, 3, 4]);
  const b = Buffer.copyBytesFrom(ab, 1, 2);
  assert.strictEqual(b.length, 2);
  assert.deepStrictEqual([...b], [2, 3]);
}

// copyBytesFrom: DataView
{
  const dv = new DataView(new ArrayBuffer(3));
  new Uint8Array(dv.buffer).set([10, 20, 30]);
  const b = Buffer.copyBytesFrom(dv);
  assert.strictEqual(b.length, 3);
  assert.deepStrictEqual([...b], [10, 20, 30]);
}

// copyBytesFrom: DataView with offset
{
  const dv = new DataView(new ArrayBuffer(4));
  new Uint8Array(dv.buffer).set([1, 2, 3, 4]);
  const b = Buffer.copyBytesFrom(dv, 2);
  assert.strictEqual(b.length, 2);
  assert.deepStrictEqual([...b], [3, 4]);
}

// copyBytesFrom: DataView with offset and length
{
  const dv = new DataView(new ArrayBuffer(5));
  new Uint8Array(dv.buffer).set([1, 2, 3, 4, 5]);
  const b = Buffer.copyBytesFrom(dv, 1, 3);
  assert.strictEqual(b.length, 3);
  assert.deepStrictEqual([...b], [2, 3, 4]);
}

// copyBytesFrom: empty ArrayBuffer
{
  const b = Buffer.copyBytesFrom(new ArrayBuffer(0));
  assert.strictEqual(b.length, 0);
}

// copyBytesFrom: empty DataView
{
  const b = Buffer.copyBytesFrom(new DataView(new ArrayBuffer(0)));
  assert.strictEqual(b.length, 0);
}

// copyBytesFrom: SharedArrayBuffer
{
  const sab = new SharedArrayBuffer(4);
  new Uint8Array(sab).set([10, 20, 30, 40]);
  const b = Buffer.copyBytesFrom(sab);
  assert.strictEqual(b.length, 4);
  assert.deepStrictEqual([...b], [10, 20, 30, 40]);
}

// copyBytesFrom: SharedArrayBuffer with offset and length
{
  const sab = new SharedArrayBuffer(4);
  new Uint8Array(sab).set([10, 20, 30, 40]);
  const b = Buffer.copyBytesFrom(sab, 1, 2);
  assert.strictEqual(b.length, 2);
  assert.deepStrictEqual([...b], [20, 30]);
}

// copyBytesFrom: Buffer input
{
  const src = Buffer.from([1, 2, 3, 4]);
  const b = Buffer.copyBytesFrom(src, 1, 2);
  src[1] = 0;
  assert.strictEqual(b.length, 2);
  assert.deepStrictEqual([...b], [2, 3]);
}

// copyBytesFrom: Int32Array preserves element-based offset/length
{
  const i32 = new Int32Array([1, 2, 3]);
  const b = Buffer.copyBytesFrom(i32, 1, 1);
  assert.strictEqual(b.length, 4);
  const view = new Int32Array(b.buffer, b.byteOffset, 1);
  assert.strictEqual(view[0], 2);
}

// Invalid encoding is allowed
Buffer.from('asd', 1);
