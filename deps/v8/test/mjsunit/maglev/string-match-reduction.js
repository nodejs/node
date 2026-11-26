// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

// String.prototype.match with a fast regexp lowers to RegExpMatchFast.

function optimize(f, ...warmup) {
  %PrepareFunctionForOptimization(f);
  for (const args of warmup) f(...args);
  %OptimizeFunctionOnNextCall(f);
}

(function testNoArguments() {
  function f(s) { return s.match(); }
  optimize(f, ["a"], ["b"]);
  assertEquals([""], [...f("a")]);
})();

(function testGlobalAndStickyLiterals() {
  function g(s) { return s.match(/^a|b/g); }
  optimize(g, ["ab"], ["zzz"]);
  assertEquals(["a", "b", "b"], g("abb"));
  assertEquals(["b"], g("xb"));
  assertNull(g("x"));
  function y(s) { return s.match(/a/y); }
  optimize(y, ["a"], ["zzz"]);
  assertEquals(["a"], [...y("ab")]);
  assertNull(y("ba"));
})();

(function testNonLiteralLastIndex() {
  function f(s, re) { return s.match(re); }
  const re = /^a/;
  optimize(f, ["a", re], ["b", re]);
  re.lastIndex = 5;
  assertEquals(["a"], [...f("a", re)]);
  assertNull(f("b", re));
  assertEquals(5, re.lastIndex);
  let calls = 0;
  re.lastIndex = { valueOf() { calls++; return 0; } };
  assertNull(f("b", re));
  assertEquals(1, calls);
})();

(function testNonStringReceiver() {
  function f(s) { return String.prototype.match.call(s, /^1/); }
  optimize(f, ["1"], ["2"]);
  assertEquals(["1"], [...f(12)]);
  assertNull(f(2));
  assertEquals(["1"], [...f({ toString() { return "1x"; } })]);
  assertThrows(() => f(null), TypeError);
  assertThrows(() => f(undefined), TypeError);
})();

(function testNonRegExpArgument() {
  function f(s, re) { return s.match(re); }
  optimize(f, ["a", /^a/], ["b", /^a/]);
  assertEquals(["a"], [...f("a", "a")]);
  assertEquals([""], [...f("a", undefined)]);
  assertEquals("custom", f("a", { [Symbol.match]() { return "custom"; } }));
  class R extends RegExp {}
  assertEquals(["a"], [...f("a", new R("^a"))]);
  const own = /^a/;
  own.exec = () => null;
  assertNull(f("a", own));
})();

(function testConstantRegExp() {
  const re = /^[a-c]x*/;
  function f(s) { return s.match(re); }
  optimize(f, ["a"], ["zzz"]);
  assertEquals(["ax"], [...f("axy")]);
  assertNull(f("dx"));
  re.compile("^d");
  assertEquals(["d"], [...f("dx")]);
  assertNull(f("ax"));
})();

(function testConstantNonRegExp() {
  const pattern = "a";
  function f(s) { return s.match(pattern); }
  optimize(f, ["a"], ["zzz"]);
  assertEquals(["a"], [...f("ba")]);
  assertNull(f("b"));
})();

(function testNonLiteralGlobal() {
  function f(s, re) { return s.match(re); }
  const re = /^a/g;
  optimize(f, ["a", re], ["b", re]);
  // A global match resets lastIndex even if nothing matches.
  re.lastIndex = 3;
  assertNull(f("b", re));
  assertEquals(0, re.lastIndex);
  assertEquals(["a"], f("ab", re));
})();

(function testNonLiteralSticky() {
  function f(s, re) { return s.match(re); }
  const re = /a/y;
  optimize(f, ["a", re], ["b", re]);
  // A sticky match starts at lastIndex.
  re.lastIndex = 1;
  assertEquals(["a"], [...f("ba", re)]);
  assertEquals(2, re.lastIndex);
  re.lastIndex = 1;
  assertNull(f("ab", re));
  assertEquals(0, re.lastIndex);
})();
