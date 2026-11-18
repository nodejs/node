// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --turbolev-non-eager-loop-peeling

function peeled_loop_truncation(a, b, c, n) {
  let x = 1;
  let res = 0;
  let i = 0;
  while (i < n) {
    %AssertPeeled();
    i = (i + 1) | 0;
    let w = (a << 1) + ((b ^ i) | 1);
    if (c) {
      res = 1;
    }
    res = (x | 0) | (w | 0);
    x = i;
  }
  return res;
}

%PrepareFunctionForOptimization(peeled_loop_truncation);
for (let i = 0; i < 10; i++) {
  assertEquals(13, peeled_loop_truncation(1, 2, true, 5));
  assertEquals(13, peeled_loop_truncation(1, 2, false, 5));
}
%OptimizeFunctionOnNextCall(peeled_loop_truncation);
assertEquals(13, peeled_loop_truncation(1, 2, true, 5));
assertOptimized(peeled_loop_truncation);

// In the peeled first iteration (n = 1), `w` is only used via `w | 0` and must
// be truncated to a wrapping Int32Add rather than kept alive in a peeled
// backedge CheckpointedJump deopt frame.
assertEquals(-2147483639, peeled_loop_truncation(1073741823, 10, true, 1));
assertOptimized(peeled_loop_truncation);
