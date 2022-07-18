// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --harmony-rab-gsab

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');


(function TestSharedArrayBuffer() {
  function createObjects() {
    const growableArrayBuffer = new SharedArrayBuffer(5, { maxByteLength: 10 });
    globalThis.growableArrayBuffer = growableArrayBuffer;
    const array1 = new Uint8Array(growableArrayBuffer);
    for (let i = 0; i < 5; i++) {
      array1[i] = i;
    }

    const arrayBuffer = new SharedArrayBuffer(5);
    globalThis.arrayBuffer = arrayBuffer;
    const array2 = new Uint8Array(arrayBuffer);
    for (let i = 0; i < 5; i++) {
      array2[i] = i;
    }
  }
  const { growableArrayBuffer, arrayBuffer } = takeAndUseWebSnapshot(createObjects, ['growableArrayBuffer', 'arrayBuffer']);
  assertEquals(growableArrayBuffer.byteLength, 5);
  assertEquals(growableArrayBuffer.maxByteLength, 10);
  assertTrue(growableArrayBuffer.growable);
  const array1 = new Uint8Array(growableArrayBuffer);
  for (let i = 0; i < 5; i++) {
    assertEquals(array1[i], i);
  }

  assertEquals(arrayBuffer.byteLength, 5);
  assertEquals(arrayBuffer.maxByteLength, 5);
  assertFalse(arrayBuffer.growable, false);
  const array2 = new Uint8Array(arrayBuffer);
  for (let i = 0; i < 5; i++) {
    assertEquals(array2[i], i);
  }
})();

(function TestArrayBuffer() {
  function createObjects() {
    const resizableArrayBuffer = new ArrayBuffer(5, {maxByteLength: 10});
    globalThis.resizableArrayBuffer = resizableArrayBuffer;
    const array1 = new Uint8Array(resizableArrayBuffer);
    for (let i = 0; i < 5; i++) {
      array1[i] = i;
    }

    const arrayBuffer = new ArrayBuffer(5);
    globalThis.arrayBuffer = arrayBuffer;
    const array2 = new Uint8Array(arrayBuffer);
    for (let i = 0; i < 5; i++) {
      array2[i] = i;
    }

    const detachedArrayBuffer = new ArrayBuffer(5);
    %ArrayBufferDetach(detachedArrayBuffer);
    globalThis.detachedArrayBuffer = detachedArrayBuffer;
  }
  const { resizableArrayBuffer, arrayBuffer, detachedArrayBuffer } = takeAndUseWebSnapshot(createObjects, ['resizableArrayBuffer', 'arrayBuffer', 'detachedArrayBuffer']);
  assertEquals(resizableArrayBuffer.byteLength, 5);
  assertEquals(resizableArrayBuffer.maxByteLength, 10);
  assertTrue(resizableArrayBuffer.resizable)
  const array1 = new Uint8Array(resizableArrayBuffer);
  for (let i = 0; i < 5; i++) {
    assertEquals(array1[i], i);
  }

  assertEquals(arrayBuffer.byteLength, 5);
  assertEquals(arrayBuffer.maxByteLength, 5);
  assertFalse(arrayBuffer.resizable)
  const array2 = new Uint8Array(arrayBuffer);
  for (let i = 0; i < 5; i++) {
    assertEquals(array2[i], i);
  }

  assertEquals(detachedArrayBuffer.byteLength, 0);
  assertEquals(detachedArrayBuffer.maxByteLength, 0);
})()
