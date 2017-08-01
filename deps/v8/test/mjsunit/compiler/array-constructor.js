// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test Array call with known Boolean.
(() => {
  function foo(x) { return Array(!!x); }

  assertEquals([true], foo(true));
  assertEquals([false], foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([true], foo(true));
  assertEquals([false], foo(false));
})();

// Test Array construct with known Boolean.
(() => {
  function foo(x) { return new Array(!!x); }

  assertEquals([true], foo(true));
  assertEquals([false], foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([true], foo(true));
  assertEquals([false], foo(false));
})();

// Test Array call with known String.
(() => {
  function foo(x) { return Array("" + x); }

  assertEquals(["a"], foo("a"));
  assertEquals(["b"], foo("b"));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(["a"], foo("a"));
  assertEquals(["b"], foo("b"));
})();

// Test Array construct with known String.
(() => {
  function foo(x) { return new Array("" + x); }

  assertEquals(["a"], foo("a"));
  assertEquals(["b"], foo("b"));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(["a"], foo("a"));
  assertEquals(["b"], foo("b"));
})();

// Test Array call with known fixed small integer.
(() => {
  function foo() { return Array(2); }

  assertEquals(2, foo().length);
  assertEquals(2, foo().length);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo().length);
})();

// Test Array construct with known fixed small integer.
(() => {
  function foo() { return new Array(2); }

  assertEquals(2, foo().length);
  assertEquals(2, foo().length);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo().length);
})();

// Test Array call with multiple parameters.
(() => {
  function foo(x, y, z) { return Array(x, y, z); }

  assertEquals([1, 2, 3], foo(1, 2, 3));
  assertEquals([1, 2, 3], foo(1, 2, 3));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([1, 2, 3], foo(1, 2, 3));
})();

// Test Array construct with multiple parameters.
(() => {
  function foo(x, y, z) { return new Array(x, y, z); }

  assertEquals([1, 2, 3], foo(1, 2, 3));
  assertEquals([1, 2, 3], foo(1, 2, 3));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([1, 2, 3], foo(1, 2, 3));
})();
