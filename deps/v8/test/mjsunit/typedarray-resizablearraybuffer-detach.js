// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab --allow-natives-syntax

"use strict";

class MyUint8Array extends Uint8Array {};

const ctors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Int32Array,
  Float32Array,
  Float64Array,
  Uint8ClampedArray,
  BigUint64Array,
  BigInt64Array,
  MyUint8Array
];

function CreateResizableArrayBuffer(byteLength, maxByteLength) {
  return new ArrayBuffer(byteLength, {maxByteLength: maxByteLength});
}

(function ConstructorThrowsIfBufferDetached() {
  const rab = CreateResizableArrayBuffer(40, 80);
  %ArrayBufferDetach(rab);

  for (let ctor of ctors) {
    assertThrows(() => { ctor(rab); }, TypeError);
    assertThrows(() => { ctor(rab, 8); }, TypeError);
    assertThrows(() => { ctor(rab, 8, 1); }, TypeError);
  }
})();

(function TypedArrayLengthAndByteLength() {
  const rab = CreateResizableArrayBuffer(40, 80);

  let tas = [];
  for (let ctor of ctors) {
    tas.push(new ctor(rab, 0, 3));
    tas.push(new ctor(rab, 8, 3));
    tas.push(new ctor(rab));
    tas.push(new ctor(rab, 8));
  }

  %ArrayBufferDetach(rab);

  for (let ta of tas) {
    assertEquals(0, ta.length);
    assertEquals(0, ta.byteLength);
  }
})();

(function AccessDetachedTypedArray() {
  const rab = CreateResizableArrayBuffer(16, 40);

  const i8a = new Int8Array(rab, 0, 4);

  // Initial values
  for (let i = 0; i < 4; ++i) {
    assertEquals(0, i8a[i]);
  }

  // Within-bounds write
  for (let i = 0; i < 4; ++i) {
    i8a[i] = i;
  }

  %ArrayBufferDetach(rab);

  // OOB read
  for (let i = 0; i < 4; ++i) {
    assertEquals(undefined, i8a[i]);
  }

  // OOB write (has no effect)
  for (let i = 0; i < 4; ++i) {
    i8a[i] = 10;
  }

  for (let i = 0; i < 4; ++i) {
    assertEquals(undefined, i8a[i]);
  }
})();

(function LoadFromOutOfScopeTypedArrayWithFeedback() {
  function ReadElement2(ta) {
    return ta[2];
  }
  %EnsureFeedbackVectorForFunction(ReadElement2);

  const rab = CreateResizableArrayBuffer(16, 40);

  const i8a = new Int8Array(rab, 0, 4);
  assertEquals(0, ReadElement2(i8a));

  // Within-bounds write
  for (let i = 0; i < 4; ++i) {
    i8a[i] = i;
  }

  %ArrayBufferDetach(rab);

  // OOB read
  for (let i = 0; i < 3; ++i) {
    assertEquals(undefined, ReadElement2(i8a));
  }
})();

(function StoreToOutOfScopeTypedArrayWithFeedback() {
  function WriteElement2(ta, i) {
    ta[2] = i;
  }
  %EnsureFeedbackVectorForFunction(WriteElement2);

  const rab = CreateResizableArrayBuffer(16, 40);

  const i8a = new Int8Array(rab, 0, 4);
  assertEquals(0, i8a[2]);

  // Within-bounds write
  for (let i = 0; i < 3; ++i) {
    WriteElement2(i8a, 3);
  }

  %ArrayBufferDetach(rab);

  // OOB write (has no effect)
  for (let i = 0; i < 3; ++i) {
    WriteElement2(i8a, 4);
  }

  // OOB read
  for (let i = 0; i < 3; ++i) {
    assertEquals(undefined, i8a[2]);
  }
})();
