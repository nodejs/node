// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestOptimizedFastExpm1MinusZero() {
  function foo() {
    return Object.is(Math.expm1(-0), -0);
  };
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();

(function TestOptimizedExpm1MinusZeroSlowPath() {
  function f(x) {
    return Object.is(Math.expm1(x), -0);
  };
  %PrepareFunctionForOptimization(f);
  function g() {
    return f(-0);
  };
  %PrepareFunctionForOptimization(g);
  f(0);
  // Compile function optimistically for numbers (with fast inlined
  // path for Math.expm1).
  %OptimizeFunctionOnNextCall(f);
  // Invalidate the optimistic assumption, deopting and marking non-number
  // input feedback in the call IC.
  f("0");
  // Optimize again, now with non-lowered call to Math.expm1.
  assertTrue(g());
  %OptimizeFunctionOnNextCall(g);
  assertTrue(g());
})();
