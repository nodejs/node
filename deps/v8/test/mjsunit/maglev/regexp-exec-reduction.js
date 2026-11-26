// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

// RegExp.prototype.exec on a receiver with the initial RegExp map lowers to
// RegExpPrototypeExecFast.

function optimize(f, ...warmup) {
  %PrepareFunctionForOptimization(f);
  for (const args of warmup) f(...args);
  %OptimizeFunctionOnNextCall(f);
}

function exec(re, s) {
  return re.exec(s);
}

(function testResult() {
  const re = /(?<x>b)c/;
  optimize(exec, [re, "abc"], [re, "zz"]);
  const m = exec(re, "abc");
  assertEquals(["bc", "b"], [...m]);
  assertEquals(1, m.index);
  assertEquals("abc", m.input);
  assertEquals("b", m.groups.x);
  assertNull(exec(re, "zz"));
  assertOptimized(exec);
})();

(function testHasIndices() {
  const re = /b/d;
  const f = (re, s) => re.exec(s);
  optimize(f, [re, "ab"], [re, "zz"]);
  assertEquals([1, 2], f(re, "ab").indices[0]);
  assertNull(f(re, "zz"));
  assertOptimized(f);
})();

(function testNonGlobalIgnoresLastIndex() {
  const re = /ab/;
  const f = (re, s) => re.exec(s);
  optimize(f, [re, "xxab"], [re, "zz"]);
  re.lastIndex = 5;
  assertEquals(2, f(re, "xxab").index);
  assertEquals(5, re.lastIndex);
  assertNull(f(re, "zz"));
  assertEquals(5, re.lastIndex);
})();

(function testGlobalAdvancesAndResetsLastIndex() {
  const re = /a/g;
  const f = (re, s) => re.exec(s);
  optimize(f, [re, "aa"]);
  re.lastIndex = 0;
  assertEquals(0, f(re, "aa").index);
  assertEquals(1, re.lastIndex);
  assertEquals(1, f(re, "aa").index);
  assertEquals(2, re.lastIndex);
  assertNull(f(re, "aa"));
  assertEquals(0, re.lastIndex);
})();

(function testSticky() {
  const re = /a/y;
  const f = (re, s) => re.exec(s);
  optimize(f, [re, "ba"]);
  re.lastIndex = 0;
  assertNull(f(re, "ba"));
  assertEquals(0, re.lastIndex);
  re.lastIndex = 1;
  assertEquals(1, f(re, "ba").index);
  assertEquals(2, re.lastIndex);
})();

(function testNonSmiAndNegativeLastIndex() {
  const re = /a/g;
  const f = (re, s) => re.exec(s);
  optimize(f, [re, "aa"]);
  re.lastIndex = -1;
  assertEquals(0, f(re, "aa").index);
  re.lastIndex = 1.5;
  assertEquals(1, f(re, "aa").index);
})();

(function testArgumentIsCoercedToString() {
  const re = /12/;
  const f = (re, s) => re.exec(s);
  optimize(f, [re, "12"]);
  assertEquals("123", f(re, 123).input);
  assertNull(f(re, undefined));
})();

(function testMissingArgumentIsCoercedToUndefined() {
  const re = /undefined/;
  const f = (re) => re.exec();
  optimize(f, [re]);
  assertEquals("undefined", f(re)[0]);
})();

(function testSubclass() {
  class MyRe extends RegExp {}
  const re = new MyRe("a");
  const f = (re, s) => re.exec(s);
  optimize(f, [re, "a"], [re, "b"]);
  assertEquals("a", f(re, "a")[0]);
  assertNull(f(re, "b"));
})();
