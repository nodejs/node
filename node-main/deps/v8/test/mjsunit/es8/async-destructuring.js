// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function assertThrowsAsync(run, errorType, message) {
  var actual;
  var hadValue = false;
  var hadError = false;
  var promise = run();

  if (typeof promise !== "object" || typeof promise.then !== "function") {
    throw new MjsUnitAssertionError(
        "Expected " + run.toString() +
        " to return a Promise, but it returned " + PrettyPrint(promise));
  }

  promise.then(function(value) { hadValue = true; actual = value; },
               function(error) { hadError = true; actual = error; });

  assertFalse(hadValue || hadError);

  %PerformMicrotaskCheckpoint();

  if (!hadError) {
    throw new MjsUnitAssertionError(
        "Expected " + run + "() to throw " + errorType.name +
        ", but did not throw.");
  }
  if (!(actual instanceof errorType))
    throw new MjsUnitAssertionError(
        "Expected " + run + "() to throw " + errorType.name +
        ", but threw '" + actual + "'");
  if (message !== void 0 && actual.message !== message)
    throw new MjsUnitAssertionError(
        "Expected " + run + "() to throw '" + message + "', but threw '" +
        actual.message + "'");
};

function assertEqualsAsync(expected, run, msg) {
  var actual;
  var hadValue = false;
  var hadError = false;
  var promise = run();

  if (typeof promise !== "object" || typeof promise.then !== "function") {
    throw new MjsUnitAssertionError(
        "Expected " + run.toString() +
        " to return a Promise, but it returned " + PrettyPrint(promise));
  }

  promise.then(function(value) { hadValue = true; actual = value; },
               function(error) { hadError = true; actual = error; });

  assertFalse(hadValue || hadError);

  %PerformMicrotaskCheckpoint();

  if (hadError) throw actual;

  assertTrue(
      hadValue, "Expected '" + run.toString() + "' to produce a value");

  assertEquals(expected, actual, msg);
};

(function TestDefaultEvaluationOrder() {
  var y = 0;
  var z = 0;
  var w = 0;
  async function f1(x = (y = 1)) { z = 1; await undefined; w = 1; };
  assertEquals(0, y);
  assertEquals(0, z);
  assertEquals(0, w);
  f1();
  assertEquals(1, y);
  assertEquals(1, z);
  assertEquals(0, w);
  %PerformMicrotaskCheckpoint();
  assertEquals(1, y);
  assertEquals(1, z);
  assertEquals(1, w);
})();

(function TestShadowingOfParameters() {
  async function f1({x}) { var x = 2; return x }
  assertEqualsAsync(2, () => f1({x: 1}));
  async function f2({x}) { { var x = 2; } return x; }
  assertEqualsAsync(2, () => f2({x: 1}));
  async function f3({x}) { var y = x; var x = 2; return y; }
  assertEqualsAsync(1, () => f3({x: 1}));
  async function f4({x}) { { var y = x; var x = 2; } return y; }
  assertEqualsAsync(1, () => f4({x: 1}));
  async function f5({x}, g = () => x) { var x = 2; return g(); }
  assertEqualsAsync(1, () => f5({x: 1}));
  async function f6({x}, g = () => x) { { var x = 2; } return g(); }
  assertEqualsAsync(1, () => f6({x: 1}));
  async function f7({x}) { var g = () => x; var x = 2; return g(); }
  assertEqualsAsync(2, () => f7({x: 1}));
  async function f8({x}) { { var g = () => x; var x = 2; } return g(); }
  assertEqualsAsync(2, () => f8({x: 1}));
  async function f9({x}, g = () => eval("x")) { var x = 2; return g(); }
  assertEqualsAsync(1, () => f9({x: 1}));

  async function f10({x}, y) { var y; return y }
  assertEqualsAsync(2, () => f10({x: 6}, 2));
  async function f11({x}, y) { var z = y; var y = 2; return z; }
  assertEqualsAsync(1, () => f11({x: 6}, 1));
  async function f12(y, g = () => y) { var y = 2; return g(); }
  assertEqualsAsync(1, () => f12(1));
  async function f13({x}, y, [z], v) { var x, y, z; return x*y*z*v }
  assertEqualsAsync(210, () => f13({x: 2}, 3, [5], 7));

  async function f20({x}) { function x() { return 2 }; return x(); }
  assertEqualsAsync(2, () => f20({x: 1}));
  // Annex B 3.3 function hoisting is blocked by the conflicting x declaration
  async function f21({x}) { { function x() { return 2 } } return x; }
  assertEqualsAsync(1, () => f21({x: 1}));

  var g1 = async ({x}) => { var x = 2; return x };
  assertEqualsAsync(2, () => g1({x: 1}));
  var g2 = async ({x}) => { { var x = 2; } return x; };
  assertEqualsAsync(2, () => g2({x: 1}));
  var g3 = async ({x}) => { var y = x; var x = 2; return y; };
  assertEqualsAsync(1, () => g3({x: 1}));
  var g4 = async ({x}) => { { var y = x; var x = 2; } return y; };
  assertEqualsAsync(1, () => g4({x: 1}));
  var g5 = async ({x}, g = () => x) => { var x = 2; return g(); };
  assertEqualsAsync(1, () => g5({x: 1}));
  var g6 = async ({x}, g = () => x) => { { var x = 2; } return g(); };
  assertEqualsAsync(1, () => g6({x: 1}));
  var g7 = async ({x}) => { var g = () => x; var x = 2; return g(); };
  assertEqualsAsync(2, () => g7({x: 1}));
  var g8 = async ({x}) => { { var g = () => x; var x = 2; } return g(); };
  assertEqualsAsync(2, () => g8({x: 1}));
  var g9 = async ({x}, g = () => eval("x")) => { var x = 2; return g(); };
  assertEqualsAsync(1, () => g9({x: 1}));

  var g10 = async ({x}, y) => { var y; return y };
  assertEqualsAsync(2, () => g10({x: 6}, 2));
  var g11 = async ({x}, y) => { var z = y; var y = 2; return z; };
  assertEqualsAsync(1, () => g11({x: 6}, 1));
  var g12 = async (y, g = () => y) => { var y = 2; return g(); };
  assertEqualsAsync(1, () => g12(1));
  var g13 = async ({x}, y, [z], v) => { var x, y, z; return x*y*z*v };
  assertEqualsAsync(210, () => g13({x: 2}, 3, [5], 7));

  var g20 = async ({x}) => { function x() { return 2 }; return x(); }
  assertEqualsAsync(2, () => g20({x: 1}));
  var g21 = async ({x}) => { { function x() { return 2 } } return x(); }
  assertThrowsAsync(() => g21({x: 1}), TypeError);

  // These errors are not recognized in lazy parsing; see mjsunit/bugs/bug-2728.js
  assertThrows("'use strict'; (async function f(x) { let x = 0; })()", SyntaxError);
  assertThrows("'use strict'; (async function f({x}) { let x = 0; })()", SyntaxError);
  assertThrows("'use strict'; (async function f(x) { const x = 0; })()", SyntaxError);
  assertThrows("'use strict'; (async function f({x}) { const x = 0; })()", SyntaxError);

  assertThrows("'use strict'; let g = async (x) => { let x = 0; }", SyntaxError);
  assertThrows("'use strict'; let g = async ({x}) => { let x = 0; }", SyntaxError);
  assertThrows("'use strict'; let g = async (x) => { const x = 0; }", SyntaxError);
  assertThrows("'use strict'; let g = async ({x}) => { const x = 0; }", SyntaxError);
}());

(function TestDefaults() {
  async function f1(x = 1) { return x }
  assertEqualsAsync(1, () => f1());
  assertEqualsAsync(1, () => f1(undefined));
  assertEqualsAsync(2, () => f1(2));
  assertEqualsAsync(null, () => f1(null));

  async function f2(x, y = x) { return x + y; }
  assertEqualsAsync(8, () => f2(4));
  assertEqualsAsync(8, () => f2(4, undefined));
  assertEqualsAsync(6, () => f2(4, 2));

  async function f3(x = 1, y) { return x + y; }
  assertEqualsAsync(8, () => f3(5, 3));
  assertEqualsAsync(3, () => f3(undefined, 2));
  assertEqualsAsync(6, () => f3(4, 2));

  async function f4(x = () => 1) { return x() }
  assertEqualsAsync(1, () => f4());
  assertEqualsAsync(1, () => f4(undefined));
  assertEqualsAsync(2, () => f4(() => 2));
  assertThrowsAsync(() => f4(null), TypeError);

  async function f5(x, y = () => x) { return x + y(); }
  assertEqualsAsync(8, () => f5(4));
  assertEqualsAsync(8, () => f5(4, undefined));
  assertEqualsAsync(6, () => f5(4, () => 2));

  async function f6(x = {a: 1, m() { return 2 }}) { return x.a + x.m(); }
  assertEqualsAsync(3, () => f6());
  assertEqualsAsync(3, () => f6(undefined));
  assertEqualsAsync(5, () => f6({a: 2, m() { return 3 }}));

  var g1 = async (x = 1) => { return x };
  assertEqualsAsync(1, () => g1());
  assertEqualsAsync(1, () => g1(undefined));
  assertEqualsAsync(2, () => g1(2));
  assertEqualsAsync(null, () => g1(null));

  var g2 = async (x, y = x) => { return x + y; };
  assertEqualsAsync(8, () => g2(4));
  assertEqualsAsync(8, () => g2(4, undefined));
  assertEqualsAsync(6, () => g2(4, 2));

  var g3 = async (x = 1, y) => { return x + y; };
  assertEqualsAsync(8, () => g3(5, 3));
  assertEqualsAsync(3, () => g3(undefined, 2));
  assertEqualsAsync(6, () => g3(4, 2));

  var g4 = async (x = () => 1) => { return x() };
  assertEqualsAsync(1, () => g4());
  assertEqualsAsync(1, () => g4(undefined));
  assertEqualsAsync(2, () => g4(() => 2));
  assertThrowsAsync(() => g4(null), TypeError);

  var g5 = async (x, y = () => x) => { return x + y(); };
  assertEqualsAsync(8, () => g5(4));
  assertEqualsAsync(8, () => g5(4, undefined));
  assertEqualsAsync(6, () => g5(4, () => 2));

  var g6 = async (x = {a: 1, m() { return 2 }}) => { return x.a + x.m(); };
  assertEqualsAsync(3, () => g6());
  assertEqualsAsync(3, () => g6(undefined));
  assertEqualsAsync(5, () => g6({a: 2, m() { return 3 }}));
}());


(function TestEvalInParameters() {
  async function f1(x = eval(0)) { return x }
  assertEqualsAsync(0, f1);
  async function f2(x = () => eval(1)) { return x() }
  assertEqualsAsync(1, f2);
})();


(function TestParameterScopingSloppy() {
  var x = 1;

  async function f1(a = x) { var x = 2; return a; }
  assertEqualsAsync(1, f1);
  async function f2(a = x) { function x() {}; return a; }
  assertEqualsAsync(1, () => f2());
  async function f3(a = eval("x")) { var x; return a; }
  assertEqualsAsync(1, () => f3());
  async function f31(a = eval("'use strict'; x")) { var x; return a; }
  assertEqualsAsync(1, () => f31());
  async function f4(a = function() { return x }) { var x; return a(); }
  assertEqualsAsync(1, () => f4());
  async function f5(a = () => x) { var x; return a(); }
  assertEqualsAsync(1, () => f5());
  async function f6(a = () => eval("x")) { var x; return a(); }
  assertEqualsAsync(1, () => f6());
  async function f61(a = () => { 'use strict'; return eval("x") }) { var x; return a(); }
  assertEqualsAsync(1, () => f61());
  async function f62(a = () => eval("'use strict'; x")) { var x; return a(); }
  assertEqualsAsync(1, () => f62());

  var g1 = async (a = x) => { var x = 2; return a; };
  assertEqualsAsync(1, () => g1());
  var g2 = async (a = x) => { function x() {}; return a; };
  assertEqualsAsync(1, () => g2());
  var g3 = async (a = eval("x")) => { var x; return a; };
  assertEqualsAsync(1, g3);
  var g31 = async (a = eval("'use strict'; x")) => { var x; return a; };
  assertEqualsAsync(1, () => g31());
  var g4 = async (a = function() { return x }) => { var x; return a(); };
  assertEqualsAsync(1, () => g4());
  var g5 = async (a = () => x) => { var x; return a(); };
  assertEqualsAsync(1, () => g5());
  var g6 = async (a = () => eval("x")) => { var x; return a(); };
  assertEqualsAsync(1, () => g6());
  var g61 = async (a = () => { 'use strict'; return eval("x") }) => { var x; return a(); };
  assertEqualsAsync(1, () => g61());
  var g62 = async (a = () => eval("'use strict'; x")) => { var x; return a(); };
  assertEqualsAsync(1, () => g62());

  var f11 = async function f(x = f) { var f; return x; }
  assertEqualsAsync(f11, f11);
  var f12 = async function f(x = f) { function f() {}; return x; }
  assertEqualsAsync(f12, f12);
  var f13 = async function f(f = 7, x = f) { return x; }
  assertEqualsAsync(7, f13);

  var o1 = {f: async function(x = this) { return x; }};
  assertEqualsAsync(o1, () => o1.f());
  assertEqualsAsync(1, () => o1.f(1));
})();

(function TestParameterScopingStrict() {
  "use strict";
  var x = 1;

  async function f1(a = x) { let x = 2; return a; }
  assertEqualsAsync(1, () => f1());
  async function f2(a = x) { const x = 2; return a; }
  assertEqualsAsync(1, () => f2());
  async function f3(a = x) { function x() {}; return a; }
  assertEqualsAsync(1, () => f3());
  async function f4(a = eval("x")) { var x; return a; }
  assertEqualsAsync(1, () => f4());
  async function f5(a = () => eval("x")) { var x; return a(); }
  assertEqualsAsync(1, () => f5());

  var g1 = async (a = x) => { let x = 2; return a; };
  assertEqualsAsync(1, () => g1());
  var g2 = async (a = x) => { const x = 2; return a; };
  assertEqualsAsync(1, () => g2());
  var g3 = async (a = x) => { function x() {}; return a; };
  assertEqualsAsync(1, () => g3());
  var g4 = async (a = eval("x")) => { var x; return a; };
  assertEqualsAsync(1, () => g4());
  var g5 = async (a = () => eval("x")) => { var x; return a(); };
  assertEqualsAsync(1, () => g5());

  var f11 = async function f(x = f) { let f; return x; }
  assertEqualsAsync(f11, f11);
  var f12 = async function f(x = f) { const f = 0; return x; }
  assertEqualsAsync(f12, f12);
  var f13 = async function f(x = f) { function f() {}; return x; }
  assertEqualsAsync(f13, f13);
})();

(function TestSloppyEvalScoping() {
  var x = 1;

  async function f1(y = eval("var x = 2")) { with ({}) { return x; } }
  assertEqualsAsync(2, () => f1());
  async function f2(y = eval("var x = 2"), z = x) { return z; }
  assertEqualsAsync(2, () => f2());
  assertEqualsAsync(1, () => f2(0));
  async function f3(y = eval("var x = 2"), z = eval("x")) { return z; }
  assertEqualsAsync(2, () => f3());
  assertEqualsAsync(1, () => f3(0));
  async function f8(y = (eval("var x = 2"), x)) { return y; }
  assertEqualsAsync(2, () => f8());
  assertEqualsAsync(0, () => f8(0));

  async function f11(z = eval("var y = 2")) { return y; }
  assertEqualsAsync(2, () => f11());
  async function f12(z = eval("var y = 2"), b = y) { return b; }
  assertEqualsAsync(2, () => f12());
  async function f13(z = eval("var y = 2"), b = eval("y")) { return b; }
  assertEqualsAsync(2, () => f13());

  async function f21(f = () => x) { eval("var x = 2"); return f() }
  assertEqualsAsync(1, () => f21());
  assertEqualsAsync(3, () => f21(() => 3));
  async function f22(f = () => eval("x")) { eval("var x = 2"); return f() }
  assertEqualsAsync(1, () => f22());
  assertEqualsAsync(3, () => f22(() => 3));

  var g1 = async (y = eval("var x = 2")) => { with ({}) { return x; } };
  assertEqualsAsync(2, () => g1());
  var g2 = async (y = eval("var x = 2"), z = x) => { return z; };
  assertEqualsAsync(2, () => g2());
  assertEqualsAsync(1, () => g2(0));
  var g3 = async (y = eval("var x = 2"), z = eval("x")) => { return z; };
  assertEqualsAsync(2, () => g3());
  assertEqualsAsync(1, () => g3(0));
  var g8 = async (y = (eval("var x = 2"), x)) => { return y; };
  assertEqualsAsync(2, () => g8());
  assertEqualsAsync(0, () => g8(0));

  var g11 = async (z = eval("var y = 2")) => { return y; };
  assertEqualsAsync(2, () => g11());
  var g12 = async (z = eval("var y = 2"), b = y) => { return b; };
  assertEqualsAsync(2, () => g12());
  var g13 = async (z = eval("var y = 2"), b = eval("y")) => { return b; };
  assertEqualsAsync(2, () => g13());

  var g21 = async (f = () => x) => { eval("var x = 2"); return f() };
  assertEqualsAsync(1, () => g21());
  assertEqualsAsync(3, () => g21(() => 3));
  var g22 = async (f = () => eval("x")) => { eval("var x = 2"); return f() };
  assertEqualsAsync(1, () => g22());
  assertEqualsAsync(3, () => g22(() => 3));
})();


(function TestStrictEvalScoping() {
  'use strict';
  var x = 1;

  async function f1(y = eval("var x = 2")) { return x; }
  assertEqualsAsync(1, () => f1());
  async function f2(y = eval("var x = 2"), z = x) { return z; }
  assertEqualsAsync(1, () => f2());
  assertEqualsAsync(1, () => f2(0));
  async function f3(y = eval("var x = 2"), z = eval("x")) { return z; }
  assertEqualsAsync(1, () => f3());
  assertEqualsAsync(1, () => f3(0));
  async function f8(y = (eval("var x = 2"), x)) { return y; }
  assertEqualsAsync(1, () => f8());
  assertEqualsAsync(0, () => f8(0));

  async function f11(z = eval("var y = 2")) { return y; }
  assertThrowsAsync(f11, ReferenceError);
  async function f12(z = eval("var y = 2"), b = y) {}
  assertThrowsAsync(f12, ReferenceError);
  async function f13(z = eval("var y = 2"), b = eval("y")) {}
  assertThrowsAsync(f13, ReferenceError);

  async function f21(f = () => x) { eval("var x = 2"); return f() }
  assertEqualsAsync(1, () => f21());
  assertEqualsAsync(3, () => f21(() => 3));
  async function f22(f = () => eval("x")) { eval("var x = 2"); return f() }
  assertEqualsAsync(1, () => f22());
  assertEqualsAsync(3, () => f22(() => 3));
})();

(function TestParameterTDZSloppy() {
  async function f1(a = x, x) { return a }
  assertThrowsAsync(() => f1(undefined, 4), ReferenceError);
  assertEqualsAsync(4, () => f1(4, 5));
  async function f2(a = eval("x"), x) { return a }
  assertThrowsAsync(() => f2(undefined, 4), ReferenceError);
  assertEqualsAsync(4, () => f2(4, 5));
  async function f3(a = eval("'use strict'; x"), x) { return a }
  assertThrowsAsync(() => f3(undefined, 4), ReferenceError);
  assertEqualsAsync(4, () => f3(4, 5));
  async function f4(a = () => x, x) { return a() }
  assertEqualsAsync(4, () => f4(() => 4, 5));
  async function f5(a = () => eval("x"), x) { return a() }
  assertEqualsAsync(4, () => f5(() => 4, 5));
  async function f6(a = () => eval("'use strict'; x"), x) { return a() }
  assertEqualsAsync(4, () => f6(() => 4, 5));

  async function f11(a = x, x = 2) { return a }
  assertThrowsAsync(() => f11(), ReferenceError);
  assertThrowsAsync(() => f11(undefined), ReferenceError);
  assertThrowsAsync(() => f11(undefined, 4), ReferenceError);
  assertEqualsAsync(4, () => f1(4, 5));
  async function f12(a = eval("x"), x = 2) { return a }
  assertThrowsAsync(() => f12(), ReferenceError);
  assertThrowsAsync(() => f12(undefined), ReferenceError);
  assertThrowsAsync(() => f12(undefined, 4), ReferenceError);
  assertEqualsAsync(4, () => f12(4, 5));
  async function f13(a = eval("'use strict'; x"), x = 2) { return a }
  assertThrowsAsync(() => f13(), ReferenceError);
  assertThrowsAsync(() => f13(undefined), ReferenceError);
  assertThrowsAsync(() => f13(undefined, 4), ReferenceError);
  assertEqualsAsync(4, () => f13(4, 5));

  async function f21(x = function() { return a }, ...a) { return x()[0] }
  assertEqualsAsync(4, () => f21(undefined, 4));
  async function f22(x = () => a, ...a) { return x()[0] }
  assertEqualsAsync(4, () => f22(undefined, 4));
  async function f23(x = () => eval("a"), ...a) { return x()[0] }
  assertEqualsAsync(4, () => f23(undefined, 4));
  async function f24(x = () => {'use strict'; return eval("a") }, ...a) {
    return x()[0]
  }
  assertEqualsAsync(4, () => f24(undefined, 4));
  async function f25(x = () => eval("'use strict'; a"), ...a) { return x()[0] }
  assertEqualsAsync(4, () => f25(undefined, 4));

  var g1 = async (x = function() { return a }, ...a) => { return x()[0] };
  assertEqualsAsync(4, () => g1(undefined, 4));
  var g2 = async (x = () => a, ...a) => { return x()[0] };
  assertEqualsAsync(4, () => g2(undefined, 4));
})();

(function TestParameterTDZStrict() {
  "use strict";

  async function f1(a = eval("x"), x) { return a }
  assertThrowsAsync(() => f1(undefined, 4), ReferenceError);
  assertEqualsAsync(4, () => f1(4, 5));
  async function f2(a = () => eval("x"), x) { return a() }
  assertEqualsAsync(4, () => f2(() => 4, 5));

  async function f11(a = eval("x"), x = 2) { return a }
  assertThrowsAsync(() => f11(), ReferenceError);
  assertThrowsAsync(() => f11(undefined), ReferenceError);
  assertThrowsAsync(() => f11(undefined, 4), ReferenceError);
  assertEqualsAsync(4, () => f11(4, 5));

  async function f21(x = () => eval("a"), ...a) { return x()[0] }
  assertEqualsAsync(4, () => f21(undefined, 4));
})();

(function TestArgumentsForNonSimpleParameters() {
  async function f1(x = 900) { arguments[0] = 1; return x }
  assertEqualsAsync(9, () => f1(9));
  assertEqualsAsync(900, () => f1());
  async function f2(x = 1001) { x = 2; return arguments[0] }
  assertEqualsAsync(10, () => f2(10));
  assertEqualsAsync(undefined, () => f2());
}());


(function TestFunctionLength() {
   assertEquals(0, (async function(x = 1) {}).length);
   assertEquals(0, (async function(x = 1, ...a) {}).length);
   assertEquals(1, (async function(x, y = 1) {}).length);
   assertEquals(1, (async function(x, y = 1, ...a) {}).length);
   assertEquals(2, (async function(x, y, z = 1) {}).length);
   assertEquals(2, (async function(x, y, z = 1, ...a) {}).length);
   assertEquals(1, (async function(x, y = 1, z) {}).length);
   assertEquals(1, (async function(x, y = 1, z, ...a) {}).length);
   assertEquals(1, (async function(x, y = 1, z, v = 2) {}).length);
   assertEquals(1, (async function(x, y = 1, z, v = 2, ...a) {}).length);
})();

(function TestDirectiveThrows() {
  "use strict";

  assertThrows("(async function(x=1){'use strict';})", SyntaxError);
  assertThrows("(async function(a, x=1){'use strict';})", SyntaxError);
  assertThrows("(async function({x}){'use strict';})", SyntaxError);
})();
