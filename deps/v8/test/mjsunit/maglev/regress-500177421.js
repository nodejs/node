// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev-object-tracking --no-concurrent-recompilation
// Flags: --allow-natives-syntax

function test(v) {
  let arr = Array(0, v >> 1);
  let x = arr[1];
  return x;
}

for (let i = 0; i < 1000; i++) {
  test(i);
}

%PrepareFunctionForOptimization(test);
test(1);
%OptimizeMaglevOnNextCall(test);
assertEquals(1, test(2));
