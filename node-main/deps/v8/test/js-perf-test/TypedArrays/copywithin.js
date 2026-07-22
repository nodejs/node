// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('CopyWithin', [1000], [
  new Benchmark('CopyWithin-Large', false, false, 0,
                CopyWithinLarge, CopyWithinLargeSetup, CopyWithinLargeTearDown),
]);

var initialLargeFloat64Array = new Array(10000);
for (var i = 0; i < 5000; ++i) {
  initialLargeFloat64Array[i] = i;
}
initialLargeFloat64Array = new Float64Array(initialLargeFloat64Array);
var largeFloat64Array;

function CopyWithinLarge() {
  largeFloat64Array.copyWithin(5000);
}

function CopyWithinLargeSetup() {
  largeFloat64Array = new Float64Array(initialLargeFloat64Array);
}

function CopyWithinLargeTearDown() {
  for (var i = 0; i < 5000; ++i) {
    if (largeFloat64Array[i + 5000] !== i) {
      throw new TypeError("Unexpected result!\n" + largeFloat64Array);
    }
  }
  largeFloat64Array = void 0;
}
