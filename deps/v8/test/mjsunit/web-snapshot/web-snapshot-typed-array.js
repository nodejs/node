// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --harmony-rab-gsab --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestTypedArray() {
  function createObjects() {
    const int8Array = new Int8Array(3);
    for (let i = 0; i < 3; i++) {
      int8Array[i] = i;
    }
    const uint8Array = new Uint8Array(3);
    for (let i = 0; i < 3; i++) {
      uint8Array[i] = i;
    }
    const uint8ClampedArray = new Uint8ClampedArray(3);
    for (let i = 0; i < 3; i++) {
      uint8ClampedArray[i] = i;
    }
    const int16Array = new Int16Array(3);
    for (let i = 0; i < 3; i++) {
      int16Array[i] = i;
    }
    const uint16Array = new Uint16Array(3);
    for (let i = 0; i < 3; i++) {
      uint16Array[i] = i;
    }
    const int32Array = new Int32Array(3);
    for (let i = 0; i < 3; i++) {
      int32Array[i] = i;
    }
    const uint32Array = new Uint32Array(3);
    for (let i = 0; i < 3; i++) {
      uint32Array[i] = i;
    }
    const float32Array = new Float32Array(3);
    for (let i = 0; i < 3; i++) {
      float32Array[i] = i + 0.2;
    }
    const float64Array = new Float64Array(3);
    for (let i = 0; i < 3; i++) {
      float64Array[i] = i + 0.2;
    }
    const bigInt64Array = new BigInt64Array(3);
    for (let i = 0; i < 3; i++) {
      bigInt64Array[i] = BigInt(i);
    }
    const bigUint64Array = new BigUint64Array(3);
    for (let i = 0; i < 3; i++) {
      bigUint64Array[i] = BigInt(i);
    }
    globalThis.int8Array = int8Array;
    globalThis.uint8Array = uint8Array;
    globalThis.uint8ClampedArray = uint8ClampedArray;
    globalThis.int16Array = int16Array;
    globalThis.uint16Array = uint16Array;
    globalThis.int32Array = int32Array;
    globalThis.uint32Array = uint32Array;
    globalThis.float32Array = float32Array;
    globalThis.float64Array = float64Array;
    globalThis.bigInt64Array = bigInt64Array;
    globalThis.bigUint64Array = bigUint64Array;
  }
  const {
    int8Array,
    uint8Array,
    uint8ClampedArray,
    int16Array,
    uint16Array,
    int32Array,
    uint32Array,
    float32Array,
    float64Array,
    bigInt64Array,
    bigUint64Array,
  } =
      takeAndUseWebSnapshot(createObjects, [
        'int8Array',
        'uint8Array',
        'uint8ClampedArray',
        'int16Array',
        'uint16Array',
        'int32Array',
        'uint32Array',
        'float32Array',
        'float64Array',
        'bigInt64Array',
        'bigUint64Array',
      ]);
  assertNotSame(globalThis.int8Array, int8Array);
  assertEquals(int8Array.length, 3);
  for (let i = 0; i < 3; i++) {
    assertEquals(int8Array[i], i);
  }
  assertNotSame(globalThis.uint8Array, uint8Array);
  assertEquals(uint8Array.length, 3);
  for (let i = 0; i < 3; i++) {
    assertEquals(uint8Array[i], i);
  }
  assertNotSame(globalThis.uint8ClampedArray, uint8ClampedArray);
  assertEquals(uint8ClampedArray.length, 3);
  for (let i = 0; i < 3; i++) {
    assertEquals(uint8ClampedArray[i], i);
  }
  assertNotSame(globalThis.int16Array, int16Array);
  assertEquals(int16Array.length, 3);
  for (let i = 0; i < 3; i++) {
    assertEquals(int16Array[i], i);
  }
  assertNotSame(globalThis.uint16Array, uint16Array);
  assertEquals(uint16Array.length, 3);
  for (let i = 0; i < 3; i++) {
    assertEquals(uint16Array[i], i);
  }
  assertNotSame(globalThis.int32Array, int32Array);
  assertEquals(int32Array.length, 3);
  for (let i = 0; i < 3; i++) {
    assertEquals(int32Array[i], i);
  }
  assertNotSame(globalThis.uint32Array, uint32Array);
  assertEquals(uint32Array.length, 3);
  for (let i = 0; i < 3; i++) {
    assertEquals(uint32Array[i], i);
  }
  assertNotSame(globalThis.float32Array, float32Array);
  assertEquals(float32Array.length, 3);
  for (let i = 0; i < 3; i++) {
    assertEqualsDelta(float32Array[i], i + 0.2);
  }
  assertNotSame(globalThis.float64Array, float64Array);
  assertEquals(float64Array.length, 3);
  for (let i = 0; i < 3; i++) {
    assertEqualsDelta(float64Array[i], i + 0.2);
  }
  assertNotSame(globalThis.bigInt64Array, bigInt64Array);
  assertEquals(bigInt64Array.length, 3);
  for (let i = 0; i < 3; i++) {
    assertEquals(bigInt64Array[i], BigInt(i));
  }
  assertNotSame(globalThis.bigUint64Array, bigUint64Array);
  assertEquals(bigUint64Array.length, 3);
  for (let i = 0; i < 3; i++) {
    assertEquals(bigUint64Array[i], BigInt(i));
  }
})();

(function TestInt8Array() {
  function createObjects() {
    const array = new Int8Array([-129, -128, 1, 127, 128]);
    globalThis.array = array;
    const array2 = new Int8Array(array.buffer, 1, 2);
    globalThis.array2 = array2;
  }
  const {array, array2} = takeAndUseWebSnapshot(createObjects, [
    'array',
    'array2',
  ]);
  assertEquals(array.length, 5);
  assertEquals(array[0], 127);
  assertEquals(array[1], -128);
  assertEquals(array[2], 1);
  assertEquals(array[3], 127);
  assertEquals(array[4], -128);
  assertSame(array.buffer, array2.buffer);
  assertEquals(array2.length, 2);
  assertEquals(array2[0], array[1]);
  assertEquals(array2[1], array[2]);
})();

(function TestUint8Array() {
  function createObjects() {
    const array = new Uint8Array([-1, 0, 2, 255, 256]);
    globalThis.array = array;
    const array2 = new Uint8Array(array.buffer, 1, 2);
    globalThis.array2 = array2;
  }
  const {array, array2} = takeAndUseWebSnapshot(createObjects, [
    'array',
    'array2',
  ]);
  assertEquals(array.length, 5);
  assertEquals(array[0], 255);
  assertEquals(array[1], 0);
  assertEquals(array[2], 2);
  assertEquals(array[3], 255);
  assertEquals(array[4], 0);
  assertSame(array.buffer, array2.buffer);
  assertEquals(array2.length, 2);
  assertEquals(array2[0], array[1]);
  assertEquals(array2[1], array[2]);
})();

(function TestUint8ClampedArray() {
  function createObjects() {
    const array = new Uint8ClampedArray([-1, 0, 2, 255, 256]);
    globalThis.array = array;
    const array2 = new Uint8ClampedArray(array.buffer, 1, 2);
    globalThis.array2 = array2;
  }
  const {array, array2} = takeAndUseWebSnapshot(createObjects, [
    'array',
    'array2',
  ]);
  assertEquals(array.length, 5);
  assertEquals(array[0], 0);
  assertEquals(array[1], 0);
  assertEquals(array[2], 2);
  assertEquals(array[3], 255);
  assertEquals(array[4], 255);
  assertSame(array.buffer, array2.buffer);
  assertEquals(array2.length, 2);
  assertEquals(array2[0], array[1]);
  assertEquals(array2[1], array[2]);
})();

(function TestInt16Array() {
  function createObjects() {
    const array = new Int16Array([-32769, -32768, 1, 32767, 32768]);
    globalThis.array = array;
    const array2 = new Int16Array(array.buffer, 2, 2);
    globalThis.array2 = array2;
  }
  const {array, array2} = takeAndUseWebSnapshot(createObjects, [
    'array',
    'array2',
  ]);
  assertEquals(array.length, 5);
  assertEquals(array[0], 32767);
  assertEquals(array[1], -32768);
  assertEquals(array[2], 1);
  assertEquals(array[3], 32767);
  assertEquals(array[4], -32768);
  assertSame(array.buffer, array2.buffer);
  assertEquals(array2.length, 2);
  assertEquals(array2[0], array[1]);
  assertEquals(array2[1], array[2]);
})();

(function TestUint16Array() {
  function createObjects() {
    const array = new Uint16Array([-1, 0, 2, 65535, 65536]);
    globalThis.array = array;
    const array2 = new Uint16Array(array.buffer, 2, 2);
    globalThis.array2 = array2;
  }
  const {array, array2} = takeAndUseWebSnapshot(createObjects, [
    'array',
    'array2',
  ]);
  assertEquals(array.length, 5);
  assertEquals(array[0], 65535);
  assertEquals(array[1], 0);
  assertEquals(array[2], 2);
  assertEquals(array[3], 65535);
  assertEquals(array[4], 0);
  assertSame(array.buffer, array2.buffer);
  assertEquals(array2.length, 2);
  assertEquals(array2[0], array[1]);
  assertEquals(array2[1], array[2]);
})();

(function TestInt32Array() {
  function createObjects() {
    const array = new Int32Array([
      -2147483649,
      -2147483648,
      1,
      2147483647,
      2147483648,
    ]);
    globalThis.array = array;
    const array2 = new Int32Array(array.buffer, 4, 2);
    globalThis.array2 = array2;
  }
  const {array, array2} = takeAndUseWebSnapshot(createObjects, [
    'array',
    'array2',
  ]);
  assertEquals(array.length, 5);
  assertEquals(array[0], 2147483647);
  assertEquals(array[1], -2147483648);
  assertEquals(array[2], 1);
  assertEquals(array[3], 2147483647);
  assertEquals(array[4], -2147483648);
  assertSame(array.buffer, array2.buffer);
  assertEquals(array2.length, 2);
  assertEquals(array2[0], array[1]);
  assertEquals(array2[1], array[2]);
})();

(function TestUint32Array() {
  function createObjects() {
    const array = new Uint32Array([-1, 0, 2, 4294967295, 4294967296]);
    globalThis.array = array;
    const array2 = new Uint32Array(array.buffer, 4, 2);
    globalThis.array2 = array2;
  }
  const {array, array2} = takeAndUseWebSnapshot(createObjects, [
    'array',
    'array2',
  ]);
  assertEquals(array.length, 5);
  assertEquals(array[0], 4294967295);
  assertEquals(array[1], 0);
  assertEquals(array[2], 2);
  assertEquals(array[3], 4294967295);
  assertEquals(array[4], 0);
  assertSame(array.buffer, array2.buffer);
  assertEquals(array2.length, 2);
  assertEquals(array2[0], array[1]);
  assertEquals(array2[1], array[2]);
})();

(function TestBigInt64Array() {
  function createObjects() {
    const array = new BigInt64Array([
      BigInt(-(2 ** 63)) - 1n,
      BigInt(-(2 ** 63)),
      1n,
      BigInt(2 ** 63) - 1n,
      BigInt(2 ** 63),
    ]);
    globalThis.array = array;
    const array2 = new BigInt64Array(array.buffer, 8, 2);
    globalThis.array2 = array2;
  }
  const {array, array2} = takeAndUseWebSnapshot(createObjects, [
    'array',
    'array2',
  ]);
  assertEquals(array.length, 5);
  assertEquals(array[0], BigInt(2 ** 63) - 1n);
  assertEquals(array[1], BigInt(-(2 ** 63)));
  assertEquals(array[2], 1n);
  assertEquals(array[3], BigInt(2 ** 63) - 1n);
  assertEquals(array[4], BigInt(-(2 ** 63)));
  assertSame(array.buffer, array2.buffer);
  assertEquals(array2.length, 2);
  assertEquals(array2[0], array[1]);
  assertEquals(array2[1], array[2]);
})();

(function TestBigUint32Array() {
  function createObjects() {
    const array = new BigUint64Array([
      -1n,
      0n,
      2n,
      BigInt(2 ** 64) - 1n,
      BigInt(2 ** 64),
    ]);
    globalThis.array = array;
    const array2 = new BigUint64Array(array.buffer, 8, 2);
    globalThis.array2 = array2;
  }
  const {array, array2} = takeAndUseWebSnapshot(createObjects, [
    'array',
    'array2',
  ]);
  assertEquals(array.length, 5);
  assertEquals(array[0], BigInt(2 ** 64) - 1n);
  assertEquals(array[1], 0n);
  assertEquals(array[2], 2n);
  assertEquals(array[3], BigInt(2 ** 64) - 1n);
  assertEquals(array[4], 0n);
  assertSame(array.buffer, array2.buffer);
  assertEquals(array2.length, 2);
  assertEquals(array2[0], array[1]);
  assertEquals(array2[1], array[2]);
})();

(function TestResizableTypedArray() {
  function createObjects() {
    let resizableArrayBuffer = new ArrayBuffer(1024, {
      maxByteLength: 1024 * 2,
    });
    // 0 offset, auto length
    let array = new Uint32Array(resizableArrayBuffer);
    globalThis.array = array;

    // Non-0 offset, auto length
    let array2 = new Uint32Array(resizableArrayBuffer, 256);
    globalThis.array2 = array2;

    // Non-0 offset, fixed length
    let array3 = new Uint32Array(resizableArrayBuffer, 128, 4);
    globalThis.array3 = array3;
  }
  const {array, array2, array3} = takeAndUseWebSnapshot(createObjects, [
    'array',
    'array2',
    'array3',
  ]);
  assertTrue(array.buffer.resizable);
  assertEquals(array.length, 256);   // (1024 - 0) / 4
  assertEquals(array2.length, 192);  // (1024 - 256) / 4
  assertEquals(array3.length, 4);
  array.buffer.resize(1024 * 2);
  assertEquals(array.length, 512);   // (2048 - 0) / 4
  assertEquals(array2.length, 448);  // (2048 - 256) / 4
  assertEquals(array3.length, 4);
})();

(function TestGrowableTypedArray() {
  function createObjects() {
    let resizableArrayBuffer = new SharedArrayBuffer(1024, {
      maxByteLength: 1024 * 2,
    });
    // 0 offset, auto length
    let array = new Uint32Array(resizableArrayBuffer);
    globalThis.array = array;

    // Non-0 offset, auto length
    let array2 = new Uint32Array(resizableArrayBuffer, 256);
    globalThis.array2 = array2;

    // Non-0 offset, fixed length
    let array3 = new Uint32Array(resizableArrayBuffer, 128, 4);
    globalThis.array3 = array3;
  }
  const {array, array2, array3} = takeAndUseWebSnapshot(createObjects, [
    'array',
    'array2',
    'array3',
  ]);
  assertTrue(array.buffer.growable);
  assertEquals(array.length, 256);   // (1024 - 0) / 4
  assertEquals(array2.length, 192);  // (1024 - 256) / 4
  assertEquals(array3.length, 4);
  array.buffer.grow(1024 * 2);
  assertEquals(array.length, 512);   // (2048 - 0) / 4
  assertEquals(array2.length, 448);  // (2048 - 256) / 4
  assertEquals(array3.length, 4);
})();
