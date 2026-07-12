// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const intArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray
];

const floatArrayConstructors = [
  Float32Array,
  Float64Array
];

const typedArrayConstructors = [...intArrayConstructors,
                                ...floatArrayConstructors];

for (let constructor of typedArrayConstructors) {
  // Shadowing the length of a TypedArray should work for Array.p.fill,
  // but not crash it.
  let array = new constructor([2, 2]);
  assertEquals(2, array.length);

  Object.defineProperty(array, 'length', {value: 5});
  Array.prototype.fill.call(array, 5);

  assertArrayEquals([5, 5], [array[0], array[1]]);
  assertEquals(undefined, array[2]);
}
