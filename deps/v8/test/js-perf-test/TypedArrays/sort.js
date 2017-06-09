// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('Sort', [1000], [
  new Benchmark('Sort', false, false, 0,
                sortLarge, sortLargeSetup, sortLargeTearDown),
]);

var size = 3000;
var initialLargeFloat64Array = new Array(size);
for (var i = 0; i < size; ++i) {
  initialLargeFloat64Array[i] = Math.random();
}
initialLargeFloat64Array = new Float64Array(initialLargeFloat64Array);
var largeFloat64Array;

function sortLarge() {
  largeFloat64Array.sort();
}

function sortLargeSetup() {
  largeFloat64Array = new Float64Array(initialLargeFloat64Array);
}

function sortLargeTearDown() {
  for (var i = 0; i < size - 1; ++i) {
    if (largeFloat64Array[i] > largeFloat64Array[i+1]) {
      throw new TypeError("Unexpected result!\n" + largeFloat64Array);
    }
  }
  largeFloat64Array = void 0;
}
