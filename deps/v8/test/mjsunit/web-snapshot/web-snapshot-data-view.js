// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --harmony-rab-gsab --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestDataView() {
  function createObjects() {
    const buffer = new ArrayBuffer(10);
    const array1 = new DataView(buffer, 0, 5);
    const array2 = new DataView(buffer, 5, 5);
    const array3 = new DataView(buffer, 2, 5);
    for (let i = 0; i < 5; i++) {
      array1.setUint8(i, i);
      array2.setUint8(i, i);
    }
    globalThis.array1 = array1;
    globalThis.array2 = array2;
    globalThis.array3 = array3;
  }
  const {array1, array2, array3} = takeAndUseWebSnapshot(createObjects, [
    'array1',
    'array2',
    'array3'
  ]);
  assertEquals(5, array1.byteLength);
  assertEquals(0, array1.byteOffset);
  assertEquals(5, array2.byteLength);
  assertEquals(5, array2.byteOffset);
  assertEquals(5, array3.byteLength);
  assertEquals(2, array3.byteOffset);

  for (let i = 0; i < 5; i++) {
    assertEquals(i, array1.getUint8(i));
    assertEquals(i, array2.getUint8(i));
  }
  assertSame(array1.buffer, array2.buffer);
  assertSame(array1.buffer, array3.buffer);

  new DataView(array1.buffer).setUint8(2, 10);
  assertTrue(array1.getUint8(2) === 10);
  assertTrue(array3.getUint8(0) === 10);
})();

(function TestResizableDataView() {
  function createObjects() {
    let resizableArrayBuffer = new ArrayBuffer(1024, {
      maxByteLength: 1024 * 2,
    });
    // 0 offset, auto length
    let array1 = new DataView(resizableArrayBuffer);
    globalThis.array1 = array1;

    // Non-0 offset, auto length
    let array2 = new DataView(resizableArrayBuffer, 256);
    globalThis.array2 = array2;

    // Non-0 offset, fixed length
    let array3 = new DataView(resizableArrayBuffer, 128, 4);
    globalThis.array3 = array3;
  }
  const {array1, array2, array3} = takeAndUseWebSnapshot(createObjects, [
    'array1',
    'array2',
    'array3',
  ]);
  assertTrue(array1.buffer.resizable);
  assertEquals(2048, array1.buffer.maxByteLength);
  assertEquals(1024, array1.byteLength);
  assertEquals(0, array1.byteOffset, 0);
  assertEquals(768, array2.byteLength); // 1024 - 256
  assertEquals(256, array2.byteOffset);
  assertEquals(4, array3.byteLength);
  assertEquals(128, array3.byteOffset);

  array1.buffer.resize(1024 * 2);
  assertEquals(2048, array1.byteLength);
  assertEquals(1792, array2.byteLength);  // 2048 - 256
  assertEquals(4, array3.byteLength);

  assertSame(array1.buffer, array2.buffer);
  assertSame(array1.buffer, array3.buffer);
})();

(function TestGrowableDataView() {
  function createObjects() {
    let resizableArrayBuffer = new SharedArrayBuffer(1024, {
      maxByteLength: 1024 * 2,
    });
    // 0 offset, auto length
    let array1 = new DataView(resizableArrayBuffer);
    globalThis.array1 = array1;

    // Non-0 offset, auto length
    let array2 = new DataView(resizableArrayBuffer, 256);
    globalThis.array2 = array2;

    // Non-0 offset, fixed length
    let array3 = new DataView(resizableArrayBuffer, 128, 4);
    globalThis.array3 = array3;
  }
  const {array1, array2, array3} = takeAndUseWebSnapshot(createObjects, [
    'array1',
    'array2',
    'array3',
  ]);
  assertTrue(array1.buffer.growable);
  assertEquals(2048, array1.buffer.maxByteLength);
  assertEquals(1024, array1.byteLength);
  assertEquals(0, array1.byteOffset);
  assertEquals(768, array2.byteLength); // 1024 - 256
  assertEquals(256, array2.byteOffset);
  assertEquals(4, array3.byteLength);
  assertEquals(128, array3.byteOffset);

  array1.buffer.grow(1024 * 2);
  assertEquals(2048, array1.byteLength);
  assertEquals(1792, array2.byteLength);  // 2048 - 256
  assertEquals(4, array3.byteLength);

  assertSame(array1.buffer, array2.buffer);
  assertSame(array1.buffer, array3.buffer);
})();
