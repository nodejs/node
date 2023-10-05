// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

// initial simple test case
(function() {
  function foo(string) { return string.endsWith('a'); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(false, foo(''));
  assertEquals(true, foo('a'));
  assertEquals(false, foo('ab'));
  assertEquals(true, foo('cba'));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(false, foo(''));
  assertEquals(true, foo('a'));
  assertEquals(false, foo('ab'));
  assertEquals(true, foo('cba'));
  assertOptimized(foo);
})();

// simple test case with a string longer than kMaxInlineMatchSequence
(function() {
  function foo(string) { return string.endsWith('abacd'); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(false, foo(''));
  assertEquals(true, foo('aaababacd'));
  assertEquals(false, foo('ababababa'));
  assertEquals(true, foo('cbaaabacd'));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(false, foo(''));
  assertEquals(true, foo('aaabaabacd'));
  assertEquals(false, foo('ababababa'));
  assertEquals(true, foo('cbaaabacd'));
  assertOptimized(foo);
})();

// simple test case with empty values
(function() {
  function f() { return "abc".endsWith(); }

  %PrepareFunctionForOptimization(f);
  assertEquals(false, f());
  assertEquals(false, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f());
  assertOptimized(f);
})();

// test case to check if matching is proper given changing end index
(function() {
  function g(n) { return "cba ahaa yyaaa aah".endsWith("a", n); }
  %PrepareFunctionForOptimization(g);
  assertEquals(false, g(-1));
  assertEquals(false, g(0));
  assertEquals(false, g(1));
  assertEquals(false, g(2));
  assertEquals(true, g(3));
  assertEquals(false, g(6));
  assertEquals(true, g(12));
  assertEquals(true, g(16));
  %OptimizeFunctionOnNextCall(g);
  assertEquals(false, g(-1));
  assertEquals(false, g(0));
  assertEquals(false, g(1));
  assertEquals(false, g(2));
  assertEquals(true, g(3));
  assertEquals(false, g(6));
  assertEquals(true, g(12));
  assertEquals(true, g(16));
  assertOptimized(g);
})();

// test case to check if matching is proper given changing end index, with
// longer string
(function() {
  function g(n) { return "aaaa hhaaaa maaaam zzzaaaaii".endsWith("aaaa", n); }
  %PrepareFunctionForOptimization(g);
  assertEquals(false, g(-1));
  assertEquals(false, g(0));
  assertEquals(false, g(1));
  assertEquals(false, g(2));
  assertEquals(true, g(4));
  assertEquals(false, g(9));
  assertEquals(true, g(11));
  assertEquals(false, g(14));
  assertEquals(true, g(17));
  assertEquals(false, g(22));
  assertEquals(true, g(26));
  %OptimizeFunctionOnNextCall(g);
  assertEquals(false, g(-1));
  assertEquals(false, g(0));
  assertEquals(false, g(1));
  assertEquals(false, g(2));
  assertEquals(true, g(4));
  assertEquals(false, g(9));
  assertEquals(true, g(11));
  assertEquals(false, g(14));
  assertEquals(true, g(17));
  assertEquals(false, g(22));
  assertEquals(true, g(26));
  assertOptimized(g);
})();

// dynamic test cases with both changing end index and changing search string
// where the search string is shorter than kMaxInlineMatchSequence
(function() {
  function f(w, n) { return "The quick brown fox jumps over the lazy dog".endsWith(w, n) }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f('dog'));
  assertEquals(false, f('dog', 40));
  assertEquals(true, f('ver', 30));
  assertEquals(false, f('ver', 31));
  assertEquals(true, f('er ', 31));
  assertEquals(true, f('The', 3));
  assertEquals(false, f('The', 4));
  assertEquals(true, f('fox', 19));
  assertEquals(false, f('fox', 20));
  assertEquals(true, f('ck', 9));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f('dog'));
  assertEquals(false, f('dog', 40));
  assertEquals(true, f('ver', 30));
  assertEquals(false, f('ver', 31));
  assertEquals(true, f('er ', 31));
  assertEquals(true, f('The', 3));
  assertEquals(false, f('The', 4));
  assertEquals(true, f('fox', 19));
  assertEquals(false, f('fox', 20));
  assertEquals(true, f('ck', 9));
  assertOptimized(f);
})();

// dynamic test cases with both changing end index and changing search string
// where the search string is longer than kMaxInlineMatchSequence
(function() {
  function f(w, n) { return "The quick brown fox jumps over the lazy dog".endsWith(w, n) }
  %PrepareFunctionForOptimization(f);
  assertEquals(false, f('T', -1));
  assertEquals(false, f('T', 0));
  assertEquals(true, f('T', 1));
  assertEquals(true, f('lazy dog'));
  assertEquals(false, f('T', 2));
  assertEquals(true, f('Th', 2));
  assertEquals(true, f("brown", 15));
  assertEquals(false, f("brown", 16));
  assertEquals(false, f("quick", 4));
  assertEquals(true, f("quick", 9));
  assertEquals(true, f("fox jumps over", 30));
  assertEquals(false, f("fox jumps over", 31));
  assertEquals(true, f(' fox ju', 22));
  assertEquals(false, f('over t', 28));
  assertEquals(true, f('er th', 33));
  assertEquals(true, f('wn fox jumps over the lazy do', 42));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(false, f('T', -1));
  assertEquals(false, f('T', 0));
  assertEquals(true, f('T', 1));
  assertEquals(true, f('lazy dog'));
  assertEquals(false, f('T', 2));
  assertEquals(true, f('Th', 2));
  assertEquals(true, f("brown", 15));
  assertEquals(false, f("brown", 16));
  assertEquals(false, f("quick", 4));
  assertEquals(true, f("quick", 9));
  assertEquals(true, f("fox jumps over", 30));
  assertEquals(false, f("fox jumps over", 31));
  assertEquals(true, f(' fox ju', 22));
  assertEquals(false, f('over t', 28));
  assertEquals(true, f('er th', 33));
  assertEquals(true, f('wn fox jumps over the lazy do', 42));
  assertOptimized(f);
})();

// simple case to check if function is de-optimized when called with
// non-smi
(function() {
  function f(n) { return "cba".endsWith("a", n); }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f());
  assertEquals(true, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f(4294967296));
  assertUnoptimized(f);
})();

// simple case for empty string
(function() {
  function f(str) {
    return str.endsWith('');
  }

  %PrepareFunctionForOptimization(f);
  f('foo');
  f('');
  %OptimizeFunctionOnNextCall(f);
  assertEquals(f('foo'), true);
  assertEquals(f(''), true);
  assertOptimized(f);
})();
