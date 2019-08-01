// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --turbo-inlining

// Test that SpeculativeNumberEqual[SignedSmall] properly passes the
// kIdentifyZeros truncation.
(function() {
  function foo(x, y) {
    if (x * y === 0) return 0;
    return 1;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  assertOptimized(foo);
  // Even if x*y produces -0 now, it should stay optimized.
  assertEquals(0, foo(-3, 0));
  assertEquals(0, foo(0, -3));
  assertOptimized(foo);
})();

// Test that SpeculativeNumberEqual[Number] properly passes the
// kIdentifyZeros truncation.
(function() {
  // Produce a SpeculativeNumberEqual with Number feedback.
  function bar(x, y) { return x === y; }
  %EnsureFeedbackVectorForFunction(bar);
  bar(0.1, 0.5);
  bar(-0, 100);

  function foo(x, y) {
    if (bar(x * y, 0)) return 0;
    return 1;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  assertOptimized(foo);
  // Even if x*y produces -0 now, it should stay optimized.
  assertEquals(0, foo(-3, 0));
  assertEquals(0, foo(0, -3));
  assertOptimized(foo);
})();

// Test that SpeculativeNumberLessThan[SignedSmall] properly passes the
// kIdentifyZeros truncation.
(function() {
  function foo(x, y) {
    if (x * y < 0) return 0;
    return 1;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(1, -1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1, -1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  assertOptimized(foo);
  // Even if x*y produces -0 now, it should stay optimized.
  assertEquals(1, foo(-3, 0));
  assertEquals(1, foo(0, -3));
  assertOptimized(foo);
})();

// Test that SpeculativeNumberLessThan[Number] properly passes the
// kIdentifyZeros truncation.
(function() {
  // Produce a SpeculativeNumberLessThan with Number feedback.
  function bar(x, y) { return x < y; }
  %EnsureFeedbackVectorForFunction(bar);
  bar(0.1, 0.5);
  bar(-0, 100);

  function foo(x, y) {
    if (bar(x * y, 0)) return 0;
    return 1;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(1, -1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1, -1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  assertOptimized(foo);
  // Even if x*y produces -0 now, it should stay optimized.
  assertEquals(1, foo(-3, 0));
  assertEquals(1, foo(0, -3));
  assertOptimized(foo);
})();

// Test that SpeculativeNumberLessThanOrEqual[SignedSmall] properly passes the
// kIdentifyZeros truncation.
(function() {
  function foo(x, y) {
    if (x * y <= 0) return 0;
    return 1;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  assertOptimized(foo);
  // Even if x*y produces -0 now, it should stay optimized.
  assertEquals(0, foo(-3, 0));
  assertEquals(0, foo(0, -3));
  assertOptimized(foo);
})();

// Test that SpeculativeNumberLessThanOrEqual[Number] properly passes the
// kIdentifyZeros truncation.
(function() {
  // Produce a SpeculativeNumberLessThanOrEqual with Number feedback.
  function bar(x, y) { return x <= y; }
  %EnsureFeedbackVectorForFunction(bar);
  bar(0.1, 0.5);
  bar(-0, 100);

  function foo(x, y) {
    if (bar(x * y, 0)) return 0;
    return 1;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 1));
  assertEquals(1, foo(1, 2));
  assertOptimized(foo);
  // Even if x*y produces -0 now, it should stay optimized.
  assertEquals(0, foo(-3, 0));
  assertEquals(0, foo(0, -3));
  assertOptimized(foo);
})();
