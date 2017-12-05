// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  function foo(o) { return Object.is(o, -0); }
  assertTrue(foo(-0));
  assertFalse(foo(0));
  assertFalse(foo(NaN));
  assertFalse(foo(''));
  assertFalse(foo([]));
  assertFalse(foo({}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(-0));
  assertFalse(foo(0));
  assertFalse(foo(NaN));
  assertFalse(foo(''));
  assertFalse(foo([]));
  assertFalse(foo({}));
})();

(function() {
  function foo(o) { return Object.is(-0, o); }
  assertTrue(foo(-0));
  assertFalse(foo(0));
  assertFalse(foo(NaN));
  assertFalse(foo(''));
  assertFalse(foo([]));
  assertFalse(foo({}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(-0));
  assertFalse(foo(0));
  assertFalse(foo(NaN));
  assertFalse(foo(''));
  assertFalse(foo([]));
  assertFalse(foo({}));
})();

(function() {
  function foo(o) { return Object.is(+o, -0); }
  assertTrue(foo(-0));
  assertFalse(foo(0));
  assertFalse(foo(NaN));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(-0));
  assertFalse(foo(0));
  assertFalse(foo(NaN));
})();

(function() {
  function foo(o) { return Object.is(-0, +o); }
  assertTrue(foo(-0));
  assertFalse(foo(0));
  assertFalse(foo(NaN));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(-0));
  assertFalse(foo(0));
  assertFalse(foo(NaN));
})();

(function() {
  function foo(o) { return Object.is(o, NaN); }
  assertFalse(foo(-0));
  assertFalse(foo(0));
  assertTrue(foo(NaN));
  assertFalse(foo(''));
  assertFalse(foo([]));
  assertFalse(foo({}));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-0));
  assertFalse(foo(0));
  assertTrue(foo(NaN));
  assertFalse(foo(''));
  assertFalse(foo([]));
  assertFalse(foo({}));
})();

(function() {
  function foo(o) { return Object.is(NaN, o); }
  assertFalse(foo(-0));
  assertFalse(foo(0));
  assertTrue(foo(NaN));
  assertFalse(foo(''));
  assertFalse(foo([]));
  assertFalse(foo({}));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-0));
  assertFalse(foo(0));
  assertTrue(foo(NaN));
  assertFalse(foo(''));
  assertFalse(foo([]));
  assertFalse(foo({}));
})();

(function() {
  function foo(o) { return Object.is(+o, NaN); }
  assertFalse(foo(-0));
  assertFalse(foo(0));
  assertTrue(foo(NaN));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-0));
  assertFalse(foo(0));
  assertTrue(foo(NaN));
})();

(function() {
  function foo(o) { return Object.is(NaN, +o); }
  assertFalse(foo(-0));
  assertFalse(foo(0));
  assertTrue(foo(NaN));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-0));
  assertFalse(foo(0));
  assertTrue(foo(NaN));
})();

(function() {
  function foo(o) { return Object.is(`${o}`, "foo"); }
  assertFalse(foo("bar"));
  assertTrue(foo("foo"));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo("bar"));
  assertTrue(foo("foo"));
})();

(function() {
  function foo(o) { return Object.is(o, o); }
  assertTrue(foo(-0));
  assertTrue(foo(0));
  assertTrue(foo(NaN));
  assertTrue(foo(''));
  assertTrue(foo([]));
  assertTrue(foo({}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(-0));
  assertTrue(foo(0));
  assertTrue(foo(NaN));
  assertTrue(foo(''));
  assertTrue(foo([]));
  assertTrue(foo({}));
})();
