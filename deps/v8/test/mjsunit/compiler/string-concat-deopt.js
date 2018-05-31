// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(() => {
  function f(a) {
    return "abc".concat();
  }

  assertEquals("abc", f());
  assertEquals("abc", f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abc", f());
})();

(() => {
  function f(a) {
    return "abc".concat(a);
  }

  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abcde", f("de"));
})();

(() => {
  function f(a) {
    return "abc".concat(a);
  }
  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abc1", f(1));
})();

(() => {
  function f(a) {
    return "abc".concat(a);
  }

  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  var s = "x".repeat((1 << 28) - 16);
  try {
    s = "x".repeat((1 << 30) - 1 - 24);
  } catch (e) {
  }
  assertThrows(() => f(s), RangeError);
})();


(() => {
  function f(a) {
    return "ab".concat("c");
  }

  assertEquals("abc", f());
  assertEquals("abc", f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abc", f());
})();

(() => {
  function f(a) {
    return "ab".concat("c", a);
  }

  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abcde", f("de"));
})();

(() => {
  function f(a) {
    return "ab".concat("c", a);
  }
  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("abc1", f(1));
})();

(() => {
  function f(a) {
    return "ab".concat("c", a);
  }

  assertEquals("abcde", f("de"));
  assertEquals("abcde", f("de"));
  %OptimizeFunctionOnNextCall(f);
  var s = "x".repeat((1 << 28) - 16);
  try {
    s = "x".repeat((1 << 30) - 1 - 24);
  } catch (e) {
  }
  assertThrows(() => f(s), RangeError);
})();
