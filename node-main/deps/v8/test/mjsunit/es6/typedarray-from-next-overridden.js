// Copyright 2018 the V8 project authors. All rights reserved.
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
  let ta = new constructor([1, 2, 3]);
  let it = ta[Symbol.iterator]();
  let original_next = it.__proto__["next"];
  Object.defineProperty(it.__proto__, "next", {
    value: function() {
      return {value: undefined, done: true};
    },
    configurable: true
  });
  assertEquals(0, constructor.from(ta).length);
  Object.defineProperty(it.__proto__, "next", original_next);
}
