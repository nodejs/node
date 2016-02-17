// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function ArrayMaker(x) {
  return x;
}
ArrayMaker.prototype = Array.prototype;

var arrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array,
  ArrayMaker  // Also test arrays
];

function assertArrayLikeEquals(value, expected, type) {
  assertEquals(value.__proto__, type.prototype);
  assertEquals(expected.length, value.length);
  for (var i = 0; i < value.length; ++i) {
    assertEquals(expected[i], value[i]);
  }
}

for (var constructor of arrayConstructors) {
  // Test reversing both even and odd length arrays
  var a = new constructor([1, 2, 3]);
  assertArrayLikeEquals(a.reverse(), [3, 2, 1], constructor);
  assertArrayLikeEquals(a, [3, 2, 1], constructor);

  a = new constructor([1, 2, 3, 4]);
  assertArrayLikeEquals(a.reverse(), [4, 3, 2, 1], constructor);
  assertArrayLikeEquals(a, [4, 3, 2, 1], constructor);

  if (constructor != ArrayMaker) {
    // Cannot be called on objects which are not TypedArrays
    assertThrows(function () { a.reverse.call({ length: 0 }); }, TypeError);
  } else {
    // Array.reverse works on array-like objects
    var x = { length: 2, 1: 5 };
    a.reverse.call(x);
    assertEquals(2, x.length);
    assertFalse(Object.hasOwnProperty(x, '1'));
    assertEquals(5, x[0]);
  }

  assertEquals(0, a.reverse.length);
}
