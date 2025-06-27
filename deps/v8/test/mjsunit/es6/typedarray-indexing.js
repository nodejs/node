// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array
];

var tmp = {
  [Symbol.toPrimitive]() {
    assertUnreachable("Parameter should not be processed when " +
                      "array.[[ViewedArrayBuffer]] is detached.");
    return 0;
  }
};

for (var constructor of typedArrayConstructors) {
  var array = new constructor([1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3]);

  // ----------------------------------------------------------------------
  // %TypedArray%.prototype.indexOf.
  // ----------------------------------------------------------------------

  // Negative cases.
  assertEquals(-1, new constructor([]).indexOf(1));
  assertEquals(-1, array.indexOf(4));
  assertEquals(-1, array.indexOf(3, array.length));

  assertEquals(2, array.indexOf(3));
  // Negative index out of range.
  assertEquals(0, array.indexOf(1, -17));
  // Negative index in rage.
  assertEquals(3, array.indexOf(1, -11));
  // Index in range.
  assertEquals(3, array.indexOf(1, 1));
  assertEquals(3, array.indexOf(1, 3));
  assertEquals(6, array.indexOf(1, 4));

  // Basic TypedArray function properties
  assertEquals(1, array.indexOf.length);
  assertThrows(function(){ array.indexOf.call([1], 1); }, TypeError);
  Object.defineProperty(array, 'length', {value: 1});
  assertEquals(array.indexOf(2), 1);

  // Index of infinite value
  array = new constructor([NaN, 2, 3, +Infinity, -Infinity, 5, 6]);
  if (constructor == Float32Array || constructor == Float64Array) {
    assertEquals(3, array.indexOf(Infinity));
    assertEquals(4, array.indexOf(-Infinity));
  } else {
    assertEquals(-1, array.indexOf(Infinity));
    assertEquals(-1, array.indexOf(-Infinity));
  }
  assertEquals(-1, array.indexOf(NaN));

  // Detached Operation
  var array = new constructor([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
  %ArrayBufferDetach(array.buffer);
  assertThrows(() => array.indexOf(tmp), TypeError);

  // ----------------------------------------------------------------------
  // %TypedArray%.prototype.lastIndexOf.
  // ----------------------------------------------------------------------
  array = new constructor([1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3]);

  // Negative cases.
  assertEquals(-1, new constructor([]).lastIndexOf(1));
  assertEquals(-1, array.lastIndexOf(1, -17));

  assertEquals(9, array.lastIndexOf(1));
  // Index out of range.
  assertEquals(9, array.lastIndexOf(1, array.length));
  // Index in range.
  assertEquals(0, array.lastIndexOf(1, 2));
  assertEquals(3, array.lastIndexOf(1, 4));
  assertEquals(3, array.lastIndexOf(1, 3));
  // Negative index in range.
  assertEquals(0, array.lastIndexOf(1, -11));

  // Basic TypedArray function properties
  assertEquals(1, array.lastIndexOf.length);
  assertThrows(function(){ array.lastIndexOf.call([1], 1); }, TypeError);
  Object.defineProperty(array, 'length', {value: 1});
  assertEquals(array.lastIndexOf(2), 10);
  delete array.length;

  // Index of infinite value
  array = new constructor([NaN, 2, 3, +Infinity, -Infinity, 5, 6]);
  if (constructor == Float32Array || constructor == Float64Array) {
    assertEquals(3, array.lastIndexOf(Infinity));
    assertEquals(4, array.lastIndexOf(-Infinity));
  } else {
    assertEquals(-1, array.lastIndexOf(Infinity));
    assertEquals(-1, array.lastIndexOf(-Infinity));
  }
  assertEquals(-1, array.lastIndexOf(NaN));

  // Detached Operation
  var array = new constructor([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
  %ArrayBufferDetach(array.buffer);
  assertThrows(() => array.lastIndexOf(tmp), TypeError);
}
