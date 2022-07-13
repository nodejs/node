// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('Constructor', [1000], [
  new Benchmark('Constructor', false, false, 0, constructor),
]);

function constructor() {
  new Int32Array(16);
}
