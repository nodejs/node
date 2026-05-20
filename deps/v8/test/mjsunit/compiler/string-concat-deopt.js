// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(() => {
  function f(a) {
    return "abc".concat();
  }

  %PrepareFunctionForOptimization(f);
  assertEquals("abc", f());
  assertEquals("abc", f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abc", f());
})();

(() => {
  function f(a) {
    return "abc".concat(a);
  }

  %PrepareFunctionForOptimization(f);
  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abcde", f("de"));
})();

(() => {
  function f(a) {
    return "abc".concat(a);
  }

  %PrepareFunctionForOptimization(f);
  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abc1", f(1));
})();

(() => {
  function f(a) {
    return "abc".concat(a);
  }

  %PrepareFunctionForOptimization(f);
  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  var s = "x".repeat(%StringMaxLength());
  assertThrows(() => f(s), RangeError);
})();


(() => {
  function f(a) {
    return "ab".concat("c");
  }

  %PrepareFunctionForOptimization(f);
  assertEquals("abc", f());
  assertEquals("abc", f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abc", f());
})();

(() => {
  function f(a) {
    return "ab".concat("c", a);
  }

  %PrepareFunctionForOptimization(f);
  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abcde", f("de"));
})();

(() => {
  function f(a) {
    return "ab".concat("c", a);
  }

  %PrepareFunctionForOptimization(f);
  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abc1", f(1));
})();

(() => {
  function f(a) {
    return "ab".concat("c", a);
  }

  %PrepareFunctionForOptimization(f);
  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  var s = "x".repeat(%StringMaxLength());
  assertThrows(() => f(s), RangeError);
})();
