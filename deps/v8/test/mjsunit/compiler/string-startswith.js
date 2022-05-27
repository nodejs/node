// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

(function() {
  function foo(string) { return string.startsWith('a'); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(false, foo(''));
  assertEquals(true, foo('a'));
  assertEquals(false, foo('ba'));
  assertEquals(true, foo('abc'));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(false, foo(''));
  assertEquals(true, foo('a'));
  assertEquals(false, foo('ba'));
  assertEquals(true, foo('abc'));
  assertOptimized(foo);
})();

(function() {
  function f() { return "abc".startsWith(); }

  %PrepareFunctionForOptimization(f);
  assertEquals(false, f());
  assertEquals(false, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f());
  assertOptimized(f);
})();

(function() {
  function g(n) { return "abc".startsWith("a", n); }

  %PrepareFunctionForOptimization(g);
  assertEquals(true, g(-1));
  assertEquals(true, g(0));
  assertEquals(false, g(1));
  assertEquals(false, g(2));
  assertEquals(false, g(3));
  assertEquals(false, g(4));
  %OptimizeFunctionOnNextCall(g);
  assertEquals(true, g(-1));
  assertEquals(true, g(0));
  assertEquals(false, g(1));
  assertEquals(false, g(2));
  assertEquals(false, g(3));
  assertEquals(false, g(4));
  assertOptimized(g);
})();

(function() {
  function g(n) { return "cba".startsWith("a", n); }

  %PrepareFunctionForOptimization(g);
  assertEquals(false, g(-1));
  assertEquals(false, g(0));
  assertEquals(false, g(1));
  assertEquals(true, g(2));
  assertEquals(false, g(3));
  assertEquals(false, g(4));
  %OptimizeFunctionOnNextCall(g);
  assertEquals(false, g(-1));
  assertEquals(false, g(0));
  assertEquals(false, g(1));
  assertEquals(true, g(2));
  assertEquals(false, g(3));
  assertEquals(false, g(4));
  assertOptimized(g);
})();

(function() {
  function f(n) { return "cba".startsWith("a", n); }
  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f(1073741824));
})();

(function() {
  function f(str) {
    return str.startsWith('');
  }

  %PrepareFunctionForOptimization(f);
  f('foo');
  f('');
  %OptimizeFunctionOnNextCall(f);
  assertEquals(f('foo'), true);
  assertEquals(f(''), true);
})();
