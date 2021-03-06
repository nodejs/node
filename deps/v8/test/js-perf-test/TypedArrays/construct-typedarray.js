// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('ConstructByTypedArray', [1000], [
  new Benchmark('ConstructByTypedArray', false, false, 0, constructor),
]);

var length = 1024;
var arr = new Uint8Array(length);
for (var i = 0; i < length; i++) {
  arr[i] = i;
}

function constructor() {
  new Float64Array(arr);
}
