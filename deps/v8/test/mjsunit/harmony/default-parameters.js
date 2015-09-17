// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-default-parameters --harmony-arrow-functions
// Flags: --harmony-rest-parameters


(function TestDefaults() {
  function f1(x = 1) { return x }
  assertEquals(1, f1());
  assertEquals(1, f1(undefined));
  assertEquals(2, f1(2));
  assertEquals(null, f1(null));

  function f2(x, y = x) { return x + y; }
  assertEquals(8, f2(4));
  assertEquals(8, f2(4, undefined));
  assertEquals(6, f2(4, 2));

  function f3(x = 1, y) { return x + y; }
  assertEquals(8, f3(5, 3));
  assertEquals(3, f3(undefined, 2));
  assertEquals(6, f3(4, 2));

  function f4(x = () => 1) { return x() }
  assertEquals(1, f4());
  assertEquals(1, f4(undefined));
  assertEquals(2, f4(() => 2));
  assertThrows(() => f4(null), TypeError);

  function f5(x, y = () => x) { return x + y(); }
  assertEquals(8, f5(4));
  assertEquals(8, f5(4, undefined));
  assertEquals(6, f5(4, () => 2));

  function f6(x = {a: 1, m() { return 2 }}) { return x.a + x.m(); }
  assertEquals(3, f6());
  assertEquals(3, f6(undefined));
  assertEquals(5, f6({a: 2, m() { return 3 }}));

  var g1 = (x = 1) => { return x };
  assertEquals(1, g1());
  assertEquals(1, g1(undefined));
  assertEquals(2, g1(2));
  assertEquals(null, g1(null));

  var g2 = (x, y = x) => { return x + y; };
  assertEquals(8, g2(4));
  assertEquals(8, g2(4, undefined));
  assertEquals(6, g2(4, 2));

  var g3 = (x = 1, y) => { return x + y; };
  assertEquals(8, g3(5, 3));
  assertEquals(3, g3(undefined, 2));
  assertEquals(6, g3(4, 2));

  var g4 = (x = () => 1) => { return x() };
  assertEquals(1, g4());
  assertEquals(1, g4(undefined));
  assertEquals(2, g4(() => 2));
  assertThrows(() => g4(null), TypeError);

  var g5 = (x, y = () => x) => { return x + y(); };
  assertEquals(8, g5(4));
  assertEquals(8, g5(4, undefined));
  assertEquals(6, g5(4, () => 2));

  var g6 = (x = {a: 1, m() { return 2 }}) => { return x.a + x.m(); };
  assertEquals(3, g6());
  assertEquals(3, g6(undefined));
  assertEquals(5, g6({a: 2, m() { return 3 }}));
}());


(function TestEvalInParameters() {
  function f1(x = eval(0)) { return x }
  assertEquals(0, f1());
  function f2(x = () => eval(1)) { return x() }
  assertEquals(1, f2());
})();


(function TestParameterScoping() {
  // TODO(rossberg): Add checks for variable declarations in defaults.
  var x = 1;

  function f1(a = x) { var x = 2; return a; }
  assertEquals(1, f1());
  function f2(a = x) { function x() {}; return a; }
  assertEquals(1, f2());
  function f3(a = x) { 'use strict'; let x = 2; return a; }
  assertEquals(1, f3());
  function f4(a = x) { 'use strict'; const x = 2; return a; }
  assertEquals(1, f4());
  function f5(a = x) { 'use strict'; function x() {}; return a; }
  assertEquals(1, f5());
  function f6(a = eval("x")) { var x; return a; }
  assertEquals(1, f6());
  function f61(a = eval("x")) { 'use strict'; var x; return a; }
  assertEquals(1, f61());
  function f62(a = eval("'use strict'; x")) { var x; return a; }
  assertEquals(1, f62());
  function f7(a = function() { return x }) { var x; return a(); }
  assertEquals(1, f7());
  function f8(a = () => x) { var x; return a(); }
  assertEquals(1, f8());
  function f9(a = () => eval("x")) { var x; return a(); }
  assertEquals(1, f9());
  function f91(a = () => eval("x")) { 'use strict'; var x; return a(); }
  assertEquals(1, f91());
  function f92(a = () => { 'use strict'; return eval("x") }) { var x; return a(); }
  assertEquals(1, f92());
  function f93(a = () => eval("'use strict'; x")) { var x; return a(); }
  assertEquals(1, f93());

  var g1 = (a = x) => { var x = 2; return a; };
  assertEquals(1, g1());
  var g2 = (a = x) => { function x() {}; return a; };
  assertEquals(1, g2());
  var g3 = (a = x) => { 'use strict'; let x = 2; return a; };
  assertEquals(1, g3());
  var g4 = (a = x) => { 'use strict'; const x = 2; return a; };
  assertEquals(1, g4());
  var g5 = (a = x) => { 'use strict'; function x() {}; return a; };
  assertEquals(1, g5());
  var g6 = (a = eval("x")) => { var x; return a; };
  assertEquals(1, g6());
  var g61 = (a = eval("x")) => { 'use strict'; var x; return a; };
  assertEquals(1, g61());
  var g62 = (a = eval("'use strict'; x")) => { var x; return a; };
  assertEquals(1, g62());
  var g7 = (a = function() { return x }) => { var x; return a(); };
  assertEquals(1, g7());
  var g8 = (a = () => x) => { var x; return a(); };
  assertEquals(1, g8());
  var g9 = (a = () => eval("x")) => { var x; return a(); };
  assertEquals(1, g9());
  var g91 = (a = () => eval("x")) => { 'use strict'; var x; return a(); };
  assertEquals(1, g91());
  var g92 = (a = () => { 'use strict'; return eval("x") }) => { var x; return a(); };
  assertEquals(1, g92());
  var g93 = (a = () => eval("'use strict'; x")) => { var x; return a(); };
  assertEquals(1, g93());

  var f11 = function f(x = f) { var f; return x; }
  assertSame(f11, f11());
  var f12 = function f(x = f) { function f() {}; return x; }
  assertSame(f12, f12());
  var f13 = function f(x = f) { 'use strict'; let f; return x; }
  assertSame(f13, f13());
  var f14 = function f(x = f) { 'use strict'; const f = 0; return x; }
  assertSame(f14, f14());
  var f15 = function f(x = f) { 'use strict'; function f() {}; return x; }
  assertSame(f15, f15());
  var f16 = function f(f = 7, x = f) { return x; }
  assertSame(7, f16());

  var o1 = {f: function(x = this) { return x; }};
  assertSame(o1, o1.f());
  assertSame(1, o1.f(1));
})();


(function TestParameterTDZ() {
  function f1(a = x, x) { return a }
  assertThrows(() => f1(undefined, 4), ReferenceError);
  assertEquals(4, f1(4, 5));
  function f2(a = eval("x"), x) { return a }
  assertThrows(() => f2(undefined, 4), ReferenceError);
  assertEquals(4, f2(4, 5));
  function f3(a = eval("x"), x) { 'use strict'; return a }
  assertThrows(() => f3(undefined, 4), ReferenceError);
  assertEquals(4, f3(4, 5));
  function f4(a = eval("'use strict'; x"), x) { return a }
  assertThrows(() => f4(undefined, 4), ReferenceError);
  assertEquals(4, f4(4, 5));

  function f5(a = () => x, x) { return a() }
  assertEquals(4, f5(() => 4, 5));
  function f6(a = () => eval("x"), x) { return a() }
  assertEquals(4, f6(() => 4, 5));
  function f7(a = () => eval("x"), x) { 'use strict'; return a() }
  assertEquals(4, f7(() => 4, 5));
  function f8(a = () => eval("'use strict'; x"), x) { return a() }
  assertEquals(4, f8(() => 4, 5));

  function f11(a = x, x = 2) { return a }
  assertThrows(() => f11(), ReferenceError);
  assertThrows(() => f11(undefined), ReferenceError);
  assertThrows(() => f11(undefined, 4), ReferenceError);
  assertEquals(4, f1(4, 5));
  function f12(a = eval("x"), x = 2) { return a }
  assertThrows(() => f12(), ReferenceError);
  assertThrows(() => f12(undefined), ReferenceError);
  assertThrows(() => f12(undefined, 4), ReferenceError);
  assertEquals(4, f12(4, 5));
  function f13(a = eval("x"), x = 2) { 'use strict'; return a }
  assertThrows(() => f13(), ReferenceError);
  assertThrows(() => f13(undefined), ReferenceError);
  assertThrows(() => f13(undefined, 4), ReferenceError);
  assertEquals(4, f13(4, 5));
  function f14(a = eval("'use strict'; x"), x = 2) { return a }
  assertThrows(() => f14(), ReferenceError);
  assertThrows(() => f14(undefined), ReferenceError);
  assertThrows(() => f14(undefined, 4), ReferenceError);
  assertEquals(4, f14(4, 5));

  function f34(x = function() { return a }, ...a) { return x()[0] }
  assertEquals(4, f34(undefined, 4));
  function f35(x = () => a, ...a) { return x()[0] }
  assertEquals(4, f35(undefined, 4));
  function f36(x = () => eval("a"), ...a) { return x()[0] }
  assertEquals(4, f36(undefined, 4));
  function f37(x = () => eval("a"), ...a) { 'use strict'; return x()[0] }
  assertEquals(4, f37(undefined, 4));
  function f38(x = () => { 'use strict'; return eval("a") }, ...a) { return x()[0] }
  assertEquals(4, f38(undefined, 4));
  function f39(x = () => eval("'use strict'; a"), ...a) { return x()[0] }
  assertEquals(4, f39(undefined, 4));

  var g34 = (x = function() { return a }, ...a) => { return x()[0] };
  assertEquals(4, g34(undefined, 4));
  var g35 = (x = () => a, ...a) => { return x()[0] };
  assertEquals(4, g35(undefined, 4));
})();


(function TestArgumentsForNonSimpleParameters() {
  function f1(x = 900) { arguments[0] = 1; return x }
  assertEquals(9, f1(9));
  assertEquals(900, f1());
  function f2(x = 1001) { x = 2; return arguments[0] }
  assertEquals(10, f2(10));
  assertEquals(undefined, f2());
}());


(function TestFunctionLength() {
  // TODO(rossberg): Fix arity.
  // assertEquals(0, (function(x = 1) {}).length);
  // assertEquals(0, (function(x = 1, ...a) {}).length);
  // assertEquals(1, (function(x, y = 1) {}).length);
  // assertEquals(1, (function(x, y = 1, ...a) {}).length);
  // assertEquals(2, (function(x, y, z = 1) {}).length);
  // assertEquals(2, (function(x, y, z = 1, ...a) {}).length);
  // assertEquals(1, (function(x, y = 1, z) {}).length);
  // assertEquals(1, (function(x, y = 1, z, ...a) {}).length);
  // assertEquals(1, (function(x, y = 1, z, v = 2) {}).length);
  // assertEquals(1, (function(x, y = 1, z, v = 2, ...a) {}).length);
})();
