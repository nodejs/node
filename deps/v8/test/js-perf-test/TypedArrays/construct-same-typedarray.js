// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('ConstructBySameTypedArray', [1000], [
  new Benchmark('ConstructBySameTypedArray', false, false, 0, constructor),
]);

const length = 1024;
let arr = new Uint8Array(length);
for (var i = 0; i < length; i++) {
  arr[i] = i;
}

function constructor() {
  new Uint8Array(arr);
}
