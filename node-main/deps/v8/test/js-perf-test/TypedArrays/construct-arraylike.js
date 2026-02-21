// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('ConstructArrayLike', [1000], [
  new Benchmark('ConstructArrayLike', false, false, 0, constructor),
]);

var arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];

function constructor() {
  new Int32Array(arr);
}
