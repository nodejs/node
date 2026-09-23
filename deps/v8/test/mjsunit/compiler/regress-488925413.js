// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// The warmup call only executes the `bias <= 0` branch, so `i + bias`
// never collects type feedback.  When TurboFan compiles the function,
// JSTypeHintLowering sees insufficient feedback for that addition and
// emits a soft deoptimization (LoweringResult::Exit) via
// ApplyEarlyReduction.  Before the fix, the resulting Deoptimize node
// was connected to the function exit without LoopExit nodes, which
// caused LoopFinder::HasMarkedExits() to fail and disabled loop peeling.

function f(bias) {
  var sum = 0;
  for (var i = 0; i < 10; i++) {
    // The dead branch produces a soft deopt inside the loop.
    sum += (bias > 0 ? (i + bias) : i);
  }
  return sum;
}

%PrepareFunctionForOptimization(f);
f(0);
%OptimizeFunctionOnNextCall(f);
assertEquals(45, f(0));
assertEquals(55, f(1));
