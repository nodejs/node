// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('SetFromArrayLike', [1000], [
  new Benchmark('SetFromArrayLike', false, false, 0, SetFromArrayLike),
]);

let src = [1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4];


let typed_dest = new Float32Array(16);

function SetFromArrayLike() {
  typed_dest.set(src);
}
