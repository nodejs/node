// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function foo(a, b, cond) {
  if (cond) {
    a === b;
  }
}
%PrepareFunctionForOptimization(foo);
foo("a", "b", 1); // String feedback for the ===.

function test(cond) {
  let big = 10000000000;
  const big2 = big | 0; // Truncates to int32
  return foo(big2, big2, cond);
}
%PrepareFunctionForOptimization(test);
test(0);
%OptimizeMaglevOnNextCall(test);
test(1);
