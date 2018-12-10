// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --noalways-opt

// Known receivers strict equality.
(function() {
  const a = {};
  const b = {};

  function foo() { return a === b; }

  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known receiver/null strict equality.
(function() {
  const a = {};
  const b = null;

  function foo() { return a === b; }

  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known receiver/undefined strict equality.
(function() {
  const a = {};
  const b = undefined;

  function foo() { return a === b; }

  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known receiver on one side strict equality.
(function() {
  const a = {};
  const b = {};

  function foo(a) { return a === b; }

  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
})();

// Known receiver on one side strict equality.
(function() {
  const a = {};
  const b = null;

  function foo(a) { return a === b; }

  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
})();

// Known receiver on one side strict equality.
(function() {
  const a = {};
  const b = undefined;

  function foo(a) { return a === b; }

  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
})();

// Feedback based receiver strict equality.
(function() {
  const a = {};
  const b = {};

  function foo(a, b) { return a === b; }

  assertTrue(foo(b, b));
  assertFalse(foo(a, b));
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));

  // TurboFan bakes in feedback for the left hand side.
  assertFalse(foo(null, b));
  assertUnoptimized(foo);
})();

// Feedback based receiver/null strict equality.
(function() {
  const a = {};
  const b = null;

  function foo(a, b) { return a === b; }

  assertTrue(foo(b, b));
  assertFalse(foo(a, b));
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));

  // TurboFan bakes in feedback for the left hand side.
  assertFalse(foo(1, b));
  assertUnoptimized(foo);
})();

// Feedback based receiver/undefined strict equality.
(function() {
  const a = {};
  const b = undefined;

  function foo(a, b) { return a === b; }

  assertTrue(foo(b, b));
  assertFalse(foo(a, b));
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));

  // TurboFan bakes in feedback for the left hand side.
  assertFalse(foo(1, b));
  assertUnoptimized(foo);
})();
