// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('SetFromSameType', [1000], [
  new Benchmark('SetFromSameType', false, false, 0, SetFromSameType),
]);

let src = [1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4];


let typed_src = new Float32Array(src);
let typed_dest = new Float32Array(16);

function SetFromSameType() {
  typed_dest.set(typed_src);
}
