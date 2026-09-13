// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

// RegExp.prototype.test on a receiver with the initial RegExp map and an
// untouched RegExp.prototype.exec lowers to RegExpPrototypeTestFast. lastIndex
// semantics, argument coercion and a replaced exec must all be preserved.

function test(re, s) {
  return re.test(s);
}

(function testNonGlobalIgnoresLastIndex() {
  const re = /ab/;
  %PrepareFunctionForOptimization(test);
  assertTrue(test(re, "xxab"));
  assertFalse(test(re, "zz"));
  %OptimizeFunctionOnNextCall(test);
  re.lastIndex = 5;
  assertTrue(test(re, "xxab"));
  assertEquals(5, re.lastIndex);
  assertFalse(test(re, "zz"));
  assertEquals(5, re.lastIndex);
})();

(function testGlobalAdvancesAndResetsLastIndex() {
  const re = /a/g;
  const f = (re, s) => re.test(s);
  %PrepareFunctionForOptimization(f);
  re.lastIndex = 0;
  f(re, "aa");
  %OptimizeFunctionOnNextCall(f);
  re.lastIndex = 0;
  assertTrue(f(re, "aa"));
  assertEquals(1, re.lastIndex);
  assertTrue(f(re, "aa"));
  assertEquals(2, re.lastIndex);
  assertFalse(f(re, "aa"));
  assertEquals(0, re.lastIndex);
})();

(function testSticky() {
  const re = /a/y;
  const f = (re, s) => re.test(s);
  %PrepareFunctionForOptimization(f);
  re.lastIndex = 0;
  f(re, "ba");
  %OptimizeFunctionOnNextCall(f);
  re.lastIndex = 0;
  assertFalse(f(re, "ba"));
  assertEquals(0, re.lastIndex);
  re.lastIndex = 1;
  assertTrue(f(re, "ba"));
  assertEquals(2, re.lastIndex);
})();

(function testNonSmiAndNegativeLastIndex() {
  const re = /a/g;
  const f = (re, s) => re.test(s);
  %PrepareFunctionForOptimization(f);
  re.lastIndex = 0;
  f(re, "aa");
  %OptimizeFunctionOnNextCall(f);
  // Neither a negative nor a fractional lastIndex may take the fast path
  // silently; the results must still be correct.
  re.lastIndex = -1;
  assertTrue(f(re, "aa"));
  re.lastIndex = 1.5;
  assertTrue(f(re, "aa"));
})();

(function testArgumentIsCoercedToString() {
  const re = /12/;
  %PrepareFunctionForOptimization(test);
  assertTrue(test(re, "12"));
  %OptimizeFunctionOnNextCall(test);
  assertTrue(test(re, 123));
  assertFalse(test(re, undefined));
})();

(function testExtraArgumentsAreIgnored() {
  const re = /ab/;
  const f = (re, s, extra) => re.test(s, extra);
  %PrepareFunctionForOptimization(f);
  assertTrue(f(re, "xxab", 1));
  assertFalse(f(re, "zz", 1));
  %OptimizeFunctionOnNextCall(f);
  assertTrue(f(re, "xxab", 1));
  assertFalse(f(re, "zz", 1));
})();

(function testMissingArgumentIsCoercedToUndefined() {
  const re = /undefined/;
  const f = (re) => re.test();
  %PrepareFunctionForOptimization(f);
  assertTrue(f(re));
  %OptimizeFunctionOnNextCall(f);
  assertTrue(f(re));
})();

(function testReplacedExecIsHonoured() {
  const re = /a/;
  const f = (re, s) => re.test(s);
  %PrepareFunctionForOptimization(f);
  assertTrue(f(re, "a"));
  %OptimizeFunctionOnNextCall(f);
  assertTrue(f(re, "a"));
  let called = 0;
  RegExp.prototype.exec = function (s) { called++; return null; };
  assertFalse(f(re, "a"));
  assertEquals(1, called);
  delete RegExp.prototype.exec;
})();

(function testSubclassWithOwnExec() {
  class MyRe extends RegExp { exec(s) { return null; } }
  const re = new MyRe("a");
  const f = (re, s) => re.test(s);
  %PrepareFunctionForOptimization(f);
  assertFalse(f(re, "a"));
  %OptimizeFunctionOnNextCall(f);
  assertFalse(f(re, "a"));
})();
