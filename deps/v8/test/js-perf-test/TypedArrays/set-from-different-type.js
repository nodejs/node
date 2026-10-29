// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('SetFromDifferentType', [1000], [
  new Benchmark('SetFromDifferentType', false, false, 0, SetFromDifferentType),
]);

const length = 16;


const dest_arrays = [
    new  Uint8Array(length),
    new  Int8Array(length),
    new  Uint16Array(length),
    new  Int16Array(length),
    new  Uint32Array(length),
    new  Int32Array(length),
    new  Float32Array(length),
    new  Float64Array(length),
    new  Uint8ClampedArray(length)
];

let uint8_array = new Uint8Array(length);
let int32_array = new Int32Array(length);
let float32_array = new Float32Array(length);
let float64_array = new Float64Array(length);
for (let i = 0; i < length; i++) {
  uint8_array[i] = i;
  int32_array[i] = i;
  float32_array[i] = i;
  float64_array[i] = i;
}

function SetFromDifferentType() {
  for(typed_dest of dest_arrays) {
    typed_dest.set(uint8_array);
    typed_dest.set(int32_array);
    typed_dest.set(float32_array);
    typed_dest.set(float64_array);
  }
}
