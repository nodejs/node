// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev-range-verification

// Multiplying an unbounded float64 range (which can reach infinity) by zero
// produces NaN. Range analysis must not assume the result is within [0, 0].
function test() {
  let x = 1;
  for (let i = 0; i < 1024; i++) {
    x += x;
  }
  return x * 0;
}

%PrepareFunctionForOptimization(test);
assertTrue(Number.isNaN(test()));
%OptimizeFunctionOnNextCall(test);
assertTrue(Number.isNaN(test()));
