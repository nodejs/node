// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test Array call with known Boolean.
(() => {
  function foo(x) { return Array(!!x); }

  %PrepareFunctionForOptimization(foo);
  assertEquals([true], foo(true));
  assertEquals([false], foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([true], foo(true));
  assertEquals([false], foo(false));
})();

// Test Array construct with known Boolean.
(() => {
  function foo(x) { return new Array(!!x); }

  %PrepareFunctionForOptimization(foo);
  assertEquals([true], foo(true));
  assertEquals([false], foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([true], foo(true));
  assertEquals([false], foo(false));
})();

// Test Array call with known String.
(() => {
  function foo(x) { return Array("" + x); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(["a"], foo("a"));
  assertEquals(["b"], foo("b"));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(["a"], foo("a"));
  assertEquals(["b"], foo("b"));
})();

// Test Array construct with known String.
(() => {
  function foo(x) { return new Array("" + x); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(["a"], foo("a"));
  assertEquals(["b"], foo("b"));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(["a"], foo("a"));
  assertEquals(["b"], foo("b"));
})();

// Test Array call with known fixed small integer.
(() => {
  function foo() { return Array(2); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(2, foo().length);
  assertEquals(2, foo().length);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo().length);
})();

// Test Array construct with known fixed small integer.
(() => {
  function foo() { return new Array(2); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(2, foo().length);
  assertEquals(2, foo().length);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo().length);
})();

// Test Array call with multiple parameters.
(() => {
  function foo(x, y, z) { return Array(x, y, z); }

  %PrepareFunctionForOptimization(foo);
  assertEquals([1, 2, 3], foo(1, 2, 3));
  assertEquals([1, 2, 3], foo(1, 2, 3));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([1, 2, 3], foo(1, 2, 3));
})();

// Test Array construct with multiple parameters.
(() => {
  function foo(x, y, z) { return new Array(x, y, z); }

  %PrepareFunctionForOptimization(foo);
  assertEquals([1, 2, 3], foo(1, 2, 3));
  assertEquals([1, 2, 3], foo(1, 2, 3));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([1, 2, 3], foo(1, 2, 3));
})();

// Test Array construct inside try-catch block.
(() => {
  function foo(x) { try { return new Array(x) } catch (e) { return e } }

  %PrepareFunctionForOptimization(foo);
  assertEquals([], foo(0));
  assertEquals([], foo(0));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([], foo(0));
  assertInstanceof(foo(-1), RangeError);
})();

// Packed
// Test non-extensible Array call with multiple parameters.
(() => {
  function foo(x, y, z, t) { return Object.preventExtensions(new Array(x, y, z, t)); }

  %PrepareFunctionForOptimization(foo);
  assertEquals([1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertEquals([1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertFalse(Object.isExtensible(foo(1,2,3, 'a')));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertFalse(Object.isExtensible(foo(1,2,3, 'a')));
})();

// Test sealed Array call with multiple parameters.
(() => {
  function foo(x, y, z, t) { return Object.seal(new Array(x, y, z, t)); }

  %PrepareFunctionForOptimization(foo);
  assertEquals([1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertEquals([1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertTrue(Object.isSealed(foo(1,2,3, 'a')));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertTrue(Object.isSealed(foo(1,2,3, 'a')));
})();

// Test frozen Array call with multiple parameters.
(() => {
  function foo(x, y, z, t) { return Object.freeze(new Array(x, y, z, t)); }

  %PrepareFunctionForOptimization(foo);
  assertEquals([1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertEquals([1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertTrue(Object.isFrozen(foo(1,2,3, 'a')));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertTrue(Object.isFrozen(foo(1,2,3, 'a')));
})();

// Holey
// Test non-extensible Array call with multiple parameters.
(() => {
  function foo(x, y, z, t) { return Object.preventExtensions([, x, y, z, t]); }

  %PrepareFunctionForOptimization(foo);
  assertEquals([, 1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertEquals([, 1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertFalse(Object.isExtensible(foo(1,2,3, 'a')));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([, 1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertFalse(Object.isExtensible(foo(1,2,3, 'a')));
})();

// Test sealed Array call with multiple parameters.
(() => {
  function foo(x, y, z, t) { return Object.seal([, x, y, z, t]); }

  %PrepareFunctionForOptimization(foo);
  assertEquals([, 1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertEquals([, 1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertTrue(Object.isSealed(foo(1,2,3, 'a')));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([, 1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertTrue(Object.isSealed(foo(1,2,3, 'a')));
})();

// Test frozen Array call with multiple parameters.
(() => {
  function foo(x, y, z, t) { return Object.freeze([, x, y, z, t]); }

  %PrepareFunctionForOptimization(foo);
  assertEquals([, 1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertEquals([, 1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertTrue(Object.isFrozen(foo(1,2,3, 'a')));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([, 1, 2, 3, 'a'], foo(1, 2, 3, 'a'));
  assertTrue(Object.isFrozen(foo(1,2,3, 'a')));
})();
