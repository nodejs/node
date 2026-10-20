// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(n, use_float) {
  let sum = 0x87654321;
  for (let i = 0; i < n; i++) {
    sum = (sum + (use_float ? 0.5 : i)) | 0
  }
  return sum;
}
%PrepareFunctionForOptimization(test);
// warmup: feedback = integer-only.
test(10, false);
test(10, false);
%OptimizeFunctionOnNextCall(test);
// trigger: 0.5 enters the hot path.
assertEquals(-2023406811, test(5, true));
