// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

// Test Math.imul() with no inputs.
(function() {
  function foo() { return Math.imul(); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo());
  assertEquals(0, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo());
})();

// Test Math.imul() with only one input.
(function() {
  function foo(x) { return Math.imul(x); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(1));
  assertEquals(0, foo(2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(3));
})();

// Test Math.imul() with wrong types.
(function() {
  function foo(x, y) { return Math.imul(x, y); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(null, 1));
  assertEquals(0, foo(2, undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(null, 1));
  assertEquals(0, foo(2, undefined));
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(null, 1));
  assertEquals(0, foo(2, undefined));
  assertOptimized(foo);
})();

// Test Math.imul() with signed integers (statically known).
(function() {
  function foo(x, y) { return Math.imul(x|0, y|0); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo(1, 1));
  assertEquals(2, foo(2, 1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(1, 1));
  assertEquals(2, foo(2, 1));
  assertOptimized(foo);
})();

// Test Math.imul() with unsigned integers (statically known).
(function() {
  function foo(x, y) { return Math.imul(x>>>0, y>>>0); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo(1, 1));
  assertEquals(2, foo(2, 1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(1, 1));
  assertEquals(2, foo(2, 1));
  assertOptimized(foo);
})();

// Test Math.imul() with floating-point numbers.
(function() {
  function foo(x, y) { return Math.imul(x, y); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo(1.1, 1.1));
  assertEquals(2, foo(2.1, 1.1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(1.1, 1.1));
  assertEquals(2, foo(2.1, 1.1));
  assertOptimized(foo);
})();
