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

function assertArrayLikeEquals(value, expected, type) {
  assertEquals(value.__proto__, type.prototype);
  // Don't test value.length because we mess with that;
  // instead in certain callsites we check that length
  // is set appropriately.
  for (var i = 0; i < expected.length; ++i) {
    // Use Object.is to differentiate between +-0
    assertSame(expected[i], value[i]);
  }
}

for (var constructor of typedArrayConstructors) {
  // Test default numerical sorting order
  var a = new constructor([100, 7, 45])
  assertEquals(a.sort(), a);
  assertArrayLikeEquals(a, [7, 45, 100], constructor);
  assertEquals(a.length, 3);

  // For arrays of floats, certain handling of +-0/NaN
  if (constructor === Float32Array || constructor === Float64Array) {
    var b = new constructor([+0, -0, NaN, -0, NaN, +0])
    b.sort();
    assertArrayLikeEquals(b, [-0, -0, +0, +0, NaN, NaN], constructor);
    assertEquals(b.length, 6);
  }

  // Custom sort--backwards
  a.sort(function(x, y) { return y - x; });
  assertArrayLikeEquals(a, [100, 45, 7], constructor);

  // Basic TypedArray method properties:
  // Length field is ignored
  Object.defineProperty(a, 'length', {value: 1});
  assertEquals(a.sort(), a);
  assertArrayLikeEquals(a, [7, 45, 100], constructor);
  assertEquals(a.length, 1);
  // Method doesn't work on other objects
  assertThrows(function() { a.sort.call([]); }, TypeError);

  // Do not touch elements out of byte offset
  var buf = new ArrayBuffer(constructor.BYTES_PER_ELEMENT * 3);
  var a = new constructor(buf, constructor.BYTES_PER_ELEMENT);
  var b = new constructor(buf);
  b[0] = 3; b[1] = 2; b[2] = 1;
  a.sort();
  assertArrayLikeEquals(a, [1, 2], constructor);

  // Detached Operation
  var array = new constructor([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
  %ArrayBufferNeuter(array.buffer);
  assertThrows(() => array.sort(), TypeError);
}
