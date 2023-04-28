// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --noalways-turbofan

const undetectable = %GetUndetectable();

// Known undetectable abstract equality.
(function() {
  const a = undetectable;
  const b = {};

  function foo() { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known undetectable/null abstract equality.
(function() {
  const a = undetectable;
  const b = null;

  function foo() { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();

// Known undetectable/receiver abstract equality.
(function() {
  const a = null;
  const b = undetectable;

  function foo() { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();

// Known undetectable/undefined abstract equality.
(function() {
  const a = undetectable;
  const b = undefined;

  function foo() { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();

// Known undefined/undetectable abstract equality.
(function() {
  const a = undefined;
  const b = undetectable;

  function foo() { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();

// Known undetectable on one side strict equality with receiver.
(function() {
  const a = {};
  const b = undetectable;

  function foo(a) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
})();

// Unknown undetectable on one side strict equality with receiver.
(function() {
  const a = undetectable;
  const b = {};

  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(b, b));
  assertFalse(foo(a, b));
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));
  assertTrue(foo(a, null));
  assertFalse(foo(b, null));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b, b));
  assertFalse(foo(a, b));
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));
  assertTrue(foo(a, null));
  assertFalse(foo(b, null));
  assertOptimized(foo);

  // TurboFan bakes in feedback on the inputs.
  assertFalse(foo(1));
  assertUnoptimized(foo);
})();

// Unknown undetectable on both sides.
(function() {
  const a = undetectable;

  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(a, a));
  assertTrue(foo(a, undefined));
  assertTrue(foo(undefined, a));
  assertFalse(foo(a, %GetUndetectable()));
  assertFalse(foo(%GetUndetectable(), a));
  assertFalse(foo(%GetUndetectable(), %GetUndetectable()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(a, a));
  assertTrue(foo(a, undefined));
  assertTrue(foo(undefined, a));
  assertFalse(foo(a, %GetUndetectable()));
  assertFalse(foo(%GetUndetectable(), a));
  assertFalse(foo(%GetUndetectable(), %GetUndetectable()));
  assertOptimized(foo);

  // TurboFan bakes in feedback on the inputs.
  assertFalse(foo(1));
  assertUnoptimized(foo);
})();
