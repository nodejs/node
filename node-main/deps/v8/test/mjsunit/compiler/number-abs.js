// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// Test that NumberAbs correctly deals with PositiveInteger \/ MinusZero
// and turns the -0 into a 0.
(function() {
  function foo(x) {
    x = Math.floor(x);
    x = Math.max(x, -0);
    return 1 / Math.abs(x);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(Infinity, foo(-0));
  assertEquals(Infinity, foo(-0));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(Infinity, foo(-0));
})();

// Test that NumberAbs properly passes the kIdentifyZeros truncation
// for Signed32 \/ MinusZero inputs.
(function() {
  function foo(x) {
    return Math.abs(x * -2);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(2, foo(-1));
  assertEquals(4, foo(-2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo(-1));
  assertEquals(4, foo(-2));
  assertOptimized(foo);
  // Now `foo` should stay optimized even if `x * -2` would produce `-0`.
  assertEquals(0, foo(0));
  assertOptimized(foo);
})();

// Test that NumberAbs properly passes the kIdentifyZeros truncation
// for Unsigned32 \/ MinusZero inputs.
(function() {
  function foo(x) {
    x = x | 0;
    return Math.abs(Math.max(x * -2, 0));
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(2, foo(-1));
  assertEquals(4, foo(-2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo(-1));
  assertEquals(4, foo(-2));
  assertOptimized(foo);
  // Now `foo` should stay optimized even if `x * -2` would produce `-0`.
  assertEquals(0, foo(0));
  assertOptimized(foo);
})();

// Test that NumberAbs properly passes the kIdentifyZeros truncation
// for OrderedNumber inputs.
(function() {
  function foo(x) {
    x = x | 0;
    return Math.abs(Math.min(x * -2, 2 ** 32));
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(2, foo(-1));
  assertEquals(4, foo(-2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo(-1));
  assertEquals(4, foo(-2));
  assertOptimized(foo);
  // Now `foo` should stay optimized even if `x * -2` would produce `-0`.
  assertEquals(0, foo(0));
  assertOptimized(foo);
})();
