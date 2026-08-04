// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('SubarrayNoSpecies', [1000], [
  new Benchmark('SubarrayNoSpecies', false, false, 0,
                subarray, subarraySetup, subarrayTearDown),
]);

var size = 1000;
var initialFloat64Array = new Float64Array(size);
for (var i = 0; i < size; ++i) {
  initialFloat64Array[i] = Math.random();
}
var arr;
var new_arr;

function subarray() {
  new_arr = arr.subarray(1, -1);
}

function subarraySetup() {
  arr = new Float64Array(initialFloat64Array);
}

function subarrayTearDown() {
  for (var i = 1; i < size - 1; ++i) {
    if (arr[i] != new_arr[i - 1]) {
      throw new TypeError("Unexpected result!\n" + new_arr);
    }
  }
  arr = void 0;
  new_arr = void 0;
}
