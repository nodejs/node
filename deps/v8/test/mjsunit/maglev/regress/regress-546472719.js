// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --turbofan --use-osr
// Flags: --osr-from-maglev=3 --concurrent-recompilation

function test() {
  let sum = 0;
  for (let i = 0; i < 1000; i++) {
    sum += i;
  }
  return sum;
}

%PrepareFunctionForOptimization(test);
for (let i = 0; i < 20; i++) {
  test();
}
%OptimizeMaglevOnNextCall(test, "concurrent");
for (let i = 0; i < 50; i++) {
  test();
}
