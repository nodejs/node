// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  function foo(o) { return Object.is(o, -0); }
  %PrepareFunctionForOptimization(foo);
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
  %PrepareFunctionForOptimization(foo);
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
  %PrepareFunctionForOptimization(foo);
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
  %PrepareFunctionForOptimization(foo);
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
  %PrepareFunctionForOptimization(foo);
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
  %PrepareFunctionForOptimization(foo);
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
  %PrepareFunctionForOptimization(foo);
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
  %PrepareFunctionForOptimization(foo);
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
  %PrepareFunctionForOptimization(foo);
  assertFalse(foo("bar"));
  assertTrue(foo("foo"));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo("bar"));
  assertTrue(foo("foo"));
})();

(function() {
  function foo(o) { return Object.is(String(o), "foo"); }
  %PrepareFunctionForOptimization(foo);
  assertFalse(foo("bar"));
  assertTrue(foo("foo"));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo("bar"));
  assertTrue(foo("foo"));
})();

(function() {
  function foo(o) { return Object.is(o, o); }
  %PrepareFunctionForOptimization(foo);
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

(function() {
  function foo(o) { return Object.is(o|0, 0); }
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(0));
  assertTrue(foo(-0));
  assertTrue(foo(NaN));
  assertFalse(foo(1));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(0));
  assertTrue(foo(-0));
  assertTrue(foo(NaN));
  assertFalse(foo(1));
})();

(function() {
  const s = Symbol();
  function foo() { return Object.is(s, Symbol()); }
  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();
