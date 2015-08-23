// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

for (var constructor of typedArrayConstructors) {
  // Check various variants of empty array's slicing.
  var array = new constructor(0);
  for (var i = 0; i < 7; i++) {
    assertEquals(0, array.slice(0, 0).length);
    assertEquals(0, array.slice(1, 0).length);
    assertEquals(0, array.slice(0, 1).length);
    assertEquals(0, array.slice(-1, 0).length);
  }


  // Check various forms of arguments omission.
  array = new constructor(7);

  for (var i = 0; i < 7; i++) {
    assertEquals(array, array.slice());
    assertEquals(array, array.slice(0));
    assertEquals(array, array.slice(undefined));
    assertEquals(array, array.slice("foobar"));
    assertEquals(array, array.slice(undefined, undefined));
  }


  // Check variants of negatives and positive indices.
  array = new constructor(7);

  assertEquals(7, array.slice(-100).length);
  assertEquals(3, array.slice(-3).length);
  assertEquals(3, array.slice(4).length);
  assertEquals(1, array.slice(6).length);
  assertEquals(0, array.slice(7).length);
  assertEquals(0, array.slice(8).length);
  assertEquals(0, array.slice(100).length);

  assertEquals(0, array.slice(0, -100).length);
  assertEquals(4, array.slice(0, -3).length);
  assertEquals(4, array.slice(0, 4).length);
  assertEquals(6, array.slice(0, 6).length);
  assertEquals(7, array.slice(0, 7).length);
  assertEquals(7, array.slice(0, 8).length);
  assertEquals(7, array.slice(0, 100).length);

  // Does not permit being called on other types
  assertThrows(function () {
    constructor.prototype.slice.call([], 0, 0);
  }, TypeError);

  // Check that elements are copied properly in slice
  array = new constructor([1, 2, 3, 4]);
  var slice = array.slice(1, 3);
  assertEquals(2, slice.length);
  assertEquals(2, slice[0]);
  assertEquals(3, slice[1]);
  assertTrue(slice instanceof constructor);
}
