// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Known symbols strict equality.
(function() {
  const a = Symbol("a");
  const b = Symbol("b");

  function foo() { return a === b; }

  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Known symbol on one side strict equality.
(function() {
  const a = Symbol("a");
  const b = Symbol("b");

  function foo(a) { return a === b; }

  assertTrue(foo(b));
  assertFalse(foo(a));
  assertTrue(foo(b));
  assertFalse(foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(b));
  assertFalse(foo(a));
})();

// Feedback based symbol strict equality.
(function() {
  const a = Symbol("a");
  const b = Symbol("b");

  function foo(a, b) { return a === b; }

  assertTrue(foo(b, b));
  assertFalse(foo(a, b));
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(a, a));
  assertFalse(foo(b, a));
})();
