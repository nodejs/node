// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --noalways-opt

// Known oddballs strict equality.
(function() {
  const a = null;
  const b = undefined;

  function foo() { return a === b; }

  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known oddball on one side strict equality.
(function() {
  const a = true;
  const b = false;

  function foo(a) { return a === b; }

  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
})();

// Feedback based oddball strict equality.
(function() {
  const a = null;
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
  assertFalse(foo({}, b));
  assertUnoptimized(foo);
})();
