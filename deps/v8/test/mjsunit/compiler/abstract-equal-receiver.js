// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --noalways-opt

// Known receivers abstract equality.
(function() {
  const a = {};
  const b = {};

  function foo() { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known receiver/null abstract equality.
(function() {
  const a = {};
  const b = null;

  function foo() { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known null/receiver abstract equality.
(function() {
  const a = null;
  const b = {};

  function foo() { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known receiver/undefined abstract equality.
(function() {
  const a = {};
  const b = undefined;

  function foo() { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known undefined/receiver abstract equality.
(function() {
  const a = undefined;
  const b = {};

  function foo() { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known receiver on one side strict equality.
(function() {
  const a = {};
  const b = {};

  function foo(a) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));

  // TurboFan bakes in feedback for the (unknown) left hand side.
  assertFalse(foo(null));
  assertUnoptimized(foo);
})();

// Known receiver on one side strict equality with null.
(function() {
  const a = null;
  const b = {};

  function foo(a) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));

  // TurboFan bakes in feedback for the (unknown) left hand side.
  assertFalse(foo(1));
  assertUnoptimized(foo);
})();

// Known receiver on one side strict equality with undefined.
(function() {
  const a = undefined;
  const b = {};

  function foo(a) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));

  // TurboFan bakes in feedback for the (unknown) left hand side.
  assertFalse(foo(1));
  assertUnoptimized(foo);
})();

// Known null on one side strict equality with receiver.
(function() {
  const a = {};
  const b = null;

  function foo(a) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(null));
  assertTrue(foo(undefined));
  assertOptimized(foo);

  // TurboFan doesn't need to bake in feedback, since it sees the null.
  assertFalse(foo(1));
  assertOptimized(foo);
})();

// Known undefined on one side strict equality with receiver.
(function() {
  const a = {};
  const b = undefined;

  function foo(a) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(null));
  assertTrue(foo(undefined));
  assertOptimized(foo);

  // TurboFan needs to bake in feedback, since undefined cannot
  // be context specialized.
  assertFalse(foo(1));
  assertUnoptimized(foo);
})();
