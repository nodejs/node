// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

// Variable index and constant string
(function() {
  function foo(n) { return "abc eFgH iskla iqi iiak".charAt(n); }
  %PrepareFunctionForOptimization(foo);
  assertEquals("a", foo(0));
  assertEquals("b", foo(1));
  assertEquals("c", foo(2));
  assertEquals(" ", foo(3));
  assertEquals("s", foo(10));
  assertEquals("", foo(100));
  assertEquals("", foo(-1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("a", foo(0));
  assertEquals("b", foo(1));
  assertEquals("c", foo(2));
  assertEquals(" ", foo(3));
  assertEquals("s", foo(10));
  assertOptimized(foo);
  // deoptimisation happens because of
  // index being out of bounds
  assertEquals("", foo(100));
  assertEquals("", foo(-1));
  assertUnoptimized(foo);
})();

// Variable string and constant index
(function() {
  function foo(st) { return st.charAt(1); }
  %PrepareFunctionForOptimization(foo);
  assertEquals("b", foo("abc"));
  assertEquals("a", foo("aab"));
  assertEquals("e", foo("hello"));
  assertEquals("y", foo("hya"));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("b", foo("abc"));
  assertEquals("a", foo("aab"));
  assertEquals("e", foo("hello"));
  assertEquals("y", foo("hya"));
  assertOptimized(foo);
  // deoptimisation happens because of
  // index being out of bounds
  assertEquals("", foo(""));
  assertEquals("", foo("a"));
  assertUnoptimized(foo);
})();

// Constant string and constant index
// should result in constant folding
(function() {
  function foo() { return 'abc'.charAt(1); }
  %PrepareFunctionForOptimization(foo);
  assertEquals("b", foo());
  assertEquals("b", foo());
  assertEquals("b", foo());
  assertEquals("b", foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("b", foo());
  assertEquals("b", foo());
  assertEquals("b", foo());
  assertEquals("b", foo());
  assertOptimized(foo);
})();

// Deoptimisation in case of non-smi index in
// constant folding
(function() {
  function foo() {
    const st = 'abc';
    const non_smi = 4294967297;
    return st.charAt(non_smi);
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals("", foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("", foo());
  // should not be constant folded
  // because of non-smi index
  assertUnoptimized(foo);
})();

// Test for optimisation and deopt on
// passing non-string as receiver
(function() {
  function foo(st, i) { return st.charAt(i); }
  %PrepareFunctionForOptimization(foo);
  assertEquals("b", foo("abc", 1));
  assertEquals("a", foo("aab", 0));
  assertEquals("o", foo("hello", 4));
  assertEquals("d", foo("hello world", 10));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("b", foo("abc", 1));
  assertEquals("a", foo("aab", 0));
  assertEquals("o", foo("hello", 4));
  assertEquals("d", foo("hello world", 10));
  assertOptimized(foo);
  // deoptimisation happens because of
  // receiver being not a string
  assertThrows(() => foo({}, 1), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("o", foo("hello", 4));
  assertThrows(() => foo({}, 1), TypeError);
  assertOptimized(foo);
  assertThrows(() => foo({ a: 1 }, 1), TypeError);
  assertUnoptimized(foo);
})();

// Test for optimisation and deopt on
// passing non-smi as index
(function() {
  function foo(st, i) { return st.charAt(i); }
  %PrepareFunctionForOptimization(foo);
  assertEquals("b", foo("abc", 1));
  assertEquals("a", foo("aab", 0));
  assertEquals("o", foo("hello", 4));
  assertEquals("d", foo("hello world", 10));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("b", foo("abc", 1));
  assertEquals("a", foo("aab", 0));
  assertEquals("o", foo("hello", 4));
  assertEquals("d", foo("hello world", 10));
  assertOptimized(foo);

  // deoptimisation happens because of
  // index being not a smi
  assertEquals("", foo("abc", 4294967297));
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("o", foo("hello", 4));
  assertEquals("", foo("abc", 4294967297));
  assertEquals("a", foo("abc", {}));
  assertEquals("b", foo("abc", 1.5));
  assertEquals("a", foo("abc", NaN));
  assertOptimized(foo);
})();
