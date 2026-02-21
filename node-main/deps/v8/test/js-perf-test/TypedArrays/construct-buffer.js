// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('ConstructWithBuffer', [1000], [
  new Benchmark('ConstructWithBuffer', false, false, 0, constructor),
]);

var buffer = new ArrayBuffer(64);

function constructor() {
  new Int32Array(buffer);
}
