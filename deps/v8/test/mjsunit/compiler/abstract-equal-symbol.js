// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --noalways-opt

// Known symbols abstract equality.
(function() {
  const a = Symbol("a");
  const b = Symbol("b");

  function foo() { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known symbols abstract in-equality.
(function() {
  const a = Symbol("a");
  const b = Symbol("b");

  function foo() { return a != b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();

// Known symbol on one side abstract equality.
(function() {
  const a = Symbol("a");
  const b = Symbol("b");

  function foo(a) { return a == b; }

  // Warmup
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
  assertOptimized(foo);

  // Make optimized code bail out
  assertFalse(foo("a"));
  assertUnoptimized(foo);

  // Make sure TurboFan learns the new feedback
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo("a"));
  assertOptimized(foo);
})();

// Known symbol on one side abstract in-equality.
(function() {
  const a = Symbol("a");
  const b = Symbol("b");

  function foo(a) { return a != b; }

  // Warmup
  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(b));
  assertTrue(foo(a));
  assertFalse(foo(b));
  assertTrue(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(b));
  assertTrue(foo(a));

  // Make optimized code bail out
  assertTrue(foo("a"));
  assertUnoptimized(foo);

  // Make sure TurboFan learns the new feedback
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo("a"));
  assertOptimized(foo);
})();

// Feedback based symbol abstract equality.
(function() {
  const a = Symbol("a");
  const b = Symbol("b");

  function foo(a, b) { return a == b; }

  // Warmup
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(b, b));
  assertFalse(foo(a, b));
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));

  // Make optimized code bail out
  assertFalse(foo("a", b));
  assertUnoptimized(foo);

  // Make sure TurboFan learns the new feedback
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo("a", b));
  assertOptimized(foo);
})();

// Feedback based symbol abstract in-equality.
(function() {
  const a = Symbol("a");
  const b = Symbol("b");

  function foo(a, b) { return a != b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(b, b));
  assertTrue(foo(a, b));
  assertFalse(foo(a, a));
  assertTrue(foo(b, a));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(a, a));
  assertTrue(foo(b, a));

  // Make optimized code bail out
  assertTrue(foo("a", b));
  assertUnoptimized(foo);

  // Make sure TurboFan learns the new feedback
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo("a", b));
  assertOptimized(foo);
})();
