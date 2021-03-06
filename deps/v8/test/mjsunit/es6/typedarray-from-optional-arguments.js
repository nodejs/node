// Copyright 2020 the V8 project authors. All rights reserved.
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

function assertArrayLikeEquals(value, expected, type) {
  assertEquals(value.__proto__, type.prototype);
  assertEquals(expected.length, value.length);
  for (var i = 0; i < value.length; ++i) {
    assertEquals(expected[i], value[i]);
  }
}

for (var constructor of typedArrayConstructors) {
  let ta = new constructor([1,2,3]);
  assertArrayLikeEquals(constructor.from([1,2,3]), ta, constructor);
  assertArrayLikeEquals(constructor.from([1,2,3], undefined),
                        ta, constructor);
  assertArrayLikeEquals(constructor.from([1,2,3], undefined, undefined),
                        ta, constructor);
}
