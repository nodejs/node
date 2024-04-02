// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('ConstructAllTypedArrays', [1000], [
  new Benchmark('ConstructAllTypedArrays', false, false, 0, constructor),
]);

var typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Float32Array,
  Float64Array,
  Uint8ClampedArray
];

const length = 32;
let uint8_array = new Uint8Array(length);
let int32_array = new Int32Array(length);
let float32_array = new Float32Array(length);
let float64_array = new Float64Array(length);
for (var i = 0; i < length; i++) {
  uint8_array[i] = i;
  int32_array[i] = i;
  float32_array[i] = i;
  float64_array[i] = i;
}

function constructor() {
  for (constructor of typedArrayConstructors) {
    new constructor(uint8_array);
    new constructor(int32_array);
    new constructor(float32_array);
    new constructor(float64_array);
  }
}
