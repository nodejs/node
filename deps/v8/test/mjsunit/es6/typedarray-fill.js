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
  Float64Array];

for (var constructor of typedArrayConstructors) {
  assertEquals(1, constructor.prototype.fill.length);

  assertArrayEquals(new constructor([]).fill(8), []);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8), [8, 8, 8, 8, 8]);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8, 1), [0, 8, 8, 8, 8]);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8, 10), [0, 0, 0, 0, 0]);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8, -5), [8, 8, 8, 8, 8]);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8, 1, 4), [0, 8, 8, 8, 0]);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8, 1, -1), [0, 8, 8, 8, 0]);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8, 1, 42), [0, 8, 8, 8, 8]);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8, -3, 42), [0, 0, 8, 8, 8]);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8, -3, 4), [0, 0, 8, 8, 0]);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8, -2, -1), [0, 0, 0, 8, 0]);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8, -1, -3), [0, 0, 0, 0, 0]);
  assertArrayEquals(new constructor([0, 0, 0, 0, 0]).fill(8, 0, 4), [8, 8, 8, 8, 0]);

  // Test exceptions
  assertThrows('constructor.prototype.fill.call(null)', TypeError);
  assertThrows('constructor.prototype.fill.call(undefined)', TypeError);
  assertThrows('constructor.prototype.fill.call([])', TypeError);

  // Shadowing length doesn't affect fill, unlike Array.prototype.fill
  var a = new constructor([2, 2]);
  Object.defineProperty(a, 'length', {value: 1});
  a.fill(3);
  assertArrayEquals([a[0], a[1]], [3, 3]);
  Array.prototype.fill.call(a, 4);
  assertArrayEquals([a[0], a[1]], [4, 3]);
}
