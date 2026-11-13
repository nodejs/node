// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const f1 = function f(a = eval("var f = 42; f")) {
  return [a, f];
};
assertEquals([42, 42], f1());

const f2 = function g(a = eval("function g() { return 1; }; g()")) {
  return [a, g()];
};
assertEquals([1, 1], f2());

const f3 = function h(a = eval("{ function h() { return 2; } }; h()")) {
  return [a, h()];
};
assertEquals([2, 2], f3());

const f4 = function k(a = eval("var k = 10")) {
  return typeof k;
};
assertEquals('number', f4());

const f5 = function m(a = eval("eval('var m = 1'); m")) {
  return [a, typeof m];
};
assertEquals([1, 'number'], f5());

const f6 = function* n(a = eval("var n = 4; n")) {
  yield a;
};
assertEquals(4, f6().next().value);

// Parameters and the arguments object still conflict, also across nested
// evals.
assertThrows(() => {
  const f = function p(p = eval("var p = 1")) {};
  f();
}, SyntaxError);

assertThrows(() => {
  const f = function arguments(a = eval("var arguments = 1")) {};
  f();
}, SyntaxError);

assertThrows(() => {
  function f(a = eval("eval('var arguments = 1')")) {}
  f();
}, SyntaxError);

assertThrows(() => {
  const f = function* q(a = eval("var arguments = 1")) {};
  f();
}, SyntaxError);
