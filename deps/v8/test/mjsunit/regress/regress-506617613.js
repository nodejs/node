// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// LoopVariableOptimizer used to classify SpeculativeAdditiveSafeIntegerSubtract
// as kAddition, computing wrong induction-variable bounds for safe-int
// decrementing loops.

function f(start, step) {
  let count = 0;
  for (let i = start; i > 0; i = i - step) {
    count++;
  }
  return count;
}

%PrepareFunctionForOptimization(f);
assertEquals(10, f(1231234567890, 123123456789));
assertEquals(10, f(1231234567890, 123123456789));
%OptimizeFunctionOnNextCall(f);
assertEquals(10, f(1231234567890, 123123456789));
