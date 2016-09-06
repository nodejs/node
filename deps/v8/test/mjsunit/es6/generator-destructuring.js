// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestDefaultBeforeInitializingYield() {
  var y = 0;
  var z = 0;
  function* f1(x = (y = 1)) { z = 1 };
  assertEquals(0, y);
  assertEquals(0, z);
  var gen = f1();
  assertEquals(1, y);
  assertEquals(0, z);
  gen.next();
  assertEquals(1, y);
  assertEquals(1, z);
})();

(function TestShadowingOfParameters() {
  function* f1({x}) { var x = 2; return x }
  assertEquals(2, f1({x: 1}).next().value);
  function* f2({x}) { { var x = 2; } return x; }
  assertEquals(2, f2({x: 1}).next().value);
  function* f3({x}) { var y = x; var x = 2; return y; }
  assertEquals(1, f3({x: 1}).next().value);
  function* f4({x}) { { var y = x; var x = 2; } return y; }
  assertEquals(1, f4({x: 1}).next().value);
  function* f5({x}, g = () => x) { var x = 2; return g(); }
  assertEquals(1, f5({x: 1}).next().value);
  function* f6({x}, g = () => x) { { var x = 2; } return g(); }
  assertEquals(1, f6({x: 1}).next().value);
  function* f7({x}) { var g = () => x; var x = 2; return g(); }
  assertEquals(2, f7({x: 1}).next().value);
  function* f8({x}) { { var g = () => x; var x = 2; } return g(); }
  assertEquals(2, f8({x: 1}).next().value);
  function* f9({x}, g = () => eval("x")) { var x = 2; return g(); }
  assertEquals(1, f9({x: 1}).next().value);

  function* f10({x}, y) { var y; return y }
  assertEquals(2, f10({x: 6}, 2).next().value);
  function* f11({x}, y) { var z = y; var y = 2; return z; }
  assertEquals(1, f11({x: 6}, 1).next().value);
  function* f12(y, g = () => y) { var y = 2; return g(); }
  assertEquals(1, f12(1).next().value);
  function* f13({x}, y, [z], v) { var x, y, z; return x*y*z*v }
  assertEquals(210, f13({x: 2}, 3, [5], 7).next().value);

  function* f20({x}) { function x() { return 2 }; return x(); }
  assertEquals(2, f20({x: 1}).next().value);
  // Annex B 3.3 function hoisting is blocked by the conflicting x declaration
  function* f21({x}) { { function x() { return 2 } } return x; }
  assertEquals(1, f21({x: 1}).next().value);

  assertThrows("'use strict'; function* f(x) { let x = 0; }", SyntaxError);
  assertThrows("'use strict'; function* f({x}) { let x = 0; }", SyntaxError);
  assertThrows("'use strict'; function* f(x) { const x = 0; }", SyntaxError);
  assertThrows("'use strict'; function* f({x}) { const x = 0; }", SyntaxError);
}());

(function TestDefaults() {
  function* f1(x = 1) { return x }
  assertEquals(1, f1().next().value);
  assertEquals(1, f1(undefined).next().value);
  assertEquals(2, f1(2).next().value);
  assertEquals(null, f1(null).next().value);

  function* f2(x, y = x) { return x + y; }
  assertEquals(8, f2(4).next().value);
  assertEquals(8, f2(4, undefined).next().value);
  assertEquals(6, f2(4, 2).next().value);

  function* f3(x = 1, y) { return x + y; }
  assertEquals(8, f3(5, 3).next().value);
  assertEquals(3, f3(undefined, 2).next().value);
  assertEquals(6, f3(4, 2).next().value);

  function* f4(x = () => 1) { return x() }
  assertEquals(1, f4().next().value);
  assertEquals(1, f4(undefined).next().value);
  assertEquals(2, f4(() => 2).next().value);
  assertThrows(() => f4(null).next(), TypeError);

  function* f5(x, y = () => x) { return x + y(); }
  assertEquals(8, f5(4).next().value);
  assertEquals(8, f5(4, undefined).next().value);
  assertEquals(6, f5(4, () => 2).next().value);

  function* f6(x = {a: 1, m() { return 2 }}) { return x.a + x.m(); }
  assertEquals(3, f6().next().value);
  assertEquals(3, f6(undefined).next().value);
  assertEquals(5, f6({a: 2, m() { return 3 }}).next().value);
}());


(function TestEvalInParameters() {
  function* f1(x = eval(0)) { return x }
  assertEquals(0, f1().next().value);
  function* f2(x = () => eval(1)) { return x() }
  assertEquals(1, f2().next().value);
})();


(function TestParameterScopingSloppy() {
  var x = 1;

  function* f1(a = x) { var x = 2; return a; }
  assertEquals(1, f1().next().value);
  function* f2(a = x) { function x() {}; return a; }
  assertEquals(1, f2().next().value);
  function* f3(a = eval("x")) { var x; return a; }
  assertEquals(1, f3().next().value);
  function* f31(a = eval("'use strict'; x")) { var x; return a; }
  assertEquals(1, f31().next().value);
  function* f4(a = function() { return x }) { var x; return a(); }
  assertEquals(1, f4().next().value);
  function* f5(a = () => x) { var x; return a(); }
  assertEquals(1, f5().next().value);
  function* f6(a = () => eval("x")) { var x; return a(); }
  assertEquals(1, f6().next().value);
  function* f61(a = () => { 'use strict'; return eval("x") }) { var x; return a(); }
  assertEquals(1, f61().next().value);
  function* f62(a = () => eval("'use strict'; x")) { var x; return a(); }
  assertEquals(1, f62().next().value);

  var f11 = function* f(x = f) { var f; return x; }
  assertSame(f11, f11().next().value);
  var f12 = function* f(x = f) { function f() {}; return x; }
  assertSame(f12, f12().next().value);
  var f13 = function* f(f = 7, x = f) { return x; }
  assertSame(7, f13().next().value);

  var o1 = {f: function*(x = this) { return x; }};
  assertSame(o1, o1.f().next().value);
  assertSame(1, o1.f(1).next().value);
})();

(function TestParameterScopingStrict() {
  "use strict";
  var x = 1;

  function* f1(a = x) { let x = 2; return a; }
  assertEquals(1, f1().next().value);
  function* f2(a = x) { const x = 2; return a; }
  assertEquals(1, f2().next().value);
  function* f3(a = x) { function x() {}; return a; }
  assertEquals(1, f3().next().value);
  function* f4(a = eval("x")) { var x; return a; }
  assertEquals(1, f4().next().value);
  function* f5(a = () => eval("x")) { var x; return a(); }
  assertEquals(1, f5().next().value);

  var f11 = function* f(x = f) { let f; return x; }
  assertSame(f11, f11().next().value);
  var f12 = function* f(x = f) { const f = 0; return x; }
  assertSame(f12, f12().next().value);
  var f13 = function* f(x = f) { function f() {}; return x; }
  assertSame(f13, f13().next().value);
})();

(function TestSloppyEvalScoping() {
  var x = 1;

  function* f1(y = eval("var x = 2")) { with ({}) { return x; } }
  assertEquals(1, f1().next().value);
  function* f2(y = eval("var x = 2"), z = x) { return z; }
  assertEquals(1, f2().next().value);
  assertEquals(1, f2(0).next().value);
  function* f3(y = eval("var x = 2"), z = eval("x")) { return z; }
  assertEquals(1, f3().next().value);
  assertEquals(1, f3(0).next().value);
  function* f8(y = (eval("var x = 2"), x)) { return y; }
  assertEquals(2, f8().next().value);
  assertEquals(0, f8(0).next().value);

  function* f11(z = eval("var y = 2")) { return y; }
  assertThrows(() => f11().next(), ReferenceError);
  function* f12(z = eval("var y = 2"), b = y) {}
  assertThrows(() => f12().next(), ReferenceError);
  function* f13(z = eval("var y = 2"), b = eval("y")) {}
  assertThrows(() => f13().next(), ReferenceError);

  function* f21(f = () => x) { eval("var x = 2"); return f() }
  assertEquals(1, f21().next().value);
  assertEquals(3, f21(() => 3).next().value);
  function* f22(f = () => eval("x")) { eval("var x = 2"); return f() }
  assertEquals(1, f22().next().value);
  assertEquals(3, f22(() => 3).next().value);
})();


(function TestStrictEvalScoping() {
  'use strict';
  var x = 1;

  function* f1(y = eval("var x = 2")) { return x; }
  assertEquals(1, f1().next().value);
  function* f2(y = eval("var x = 2"), z = x) { return z; }
  assertEquals(1, f2().next().value);
  assertEquals(1, f2(0).next().value);
  function* f3(y = eval("var x = 2"), z = eval("x")) { return z; }
  assertEquals(1, f3().next().value);
  assertEquals(1, f3(0).next().value);
  function* f8(y = (eval("var x = 2"), x)) { return y; }
  assertEquals(1, f8().next().value);
  assertEquals(0, f8(0).next().value);

  function* f11(z = eval("var y = 2")) { return y; }
  assertThrows(() => f11().next().value, ReferenceError);
  function* f12(z = eval("var y = 2"), b = y) {}
  assertThrows(() => f12().next().value, ReferenceError);
  function* f13(z = eval("var y = 2"), b = eval("y")) {}
  assertThrows(() => f13().next().value, ReferenceError);

  function* f21(f = () => x) { eval("var x = 2"); return f() }
  assertEquals(1, f21().next().value);
  assertEquals(3, f21(() => 3).next().value);
  function* f22(f = () => eval("x")) { eval("var x = 2"); return f() }
  assertEquals(1, f22().next().value);
  assertEquals(3, f22(() => 3).next().value);
})();

(function TestParameterTDZSloppy() {
  function* f1(a = x, x) { return a }
  assertThrows(() => f1(undefined, 4), ReferenceError);
  assertEquals(4, f1(4, 5).next().value);
  function* f2(a = eval("x"), x) { return a }
  assertThrows(() => f2(undefined, 4), ReferenceError);
  assertEquals(4, f2(4, 5).next().value);
  function* f3(a = eval("'use strict'; x"), x) { return a }
  assertThrows(() => f3(undefined, 4), ReferenceError);
  assertEquals(4, f3(4, 5).next().value);
  function* f4(a = () => x, x) { return a() }
  assertEquals(4, f4(() => 4, 5).next().value);
  function* f5(a = () => eval("x"), x) { return a() }
  assertEquals(4, f5(() => 4, 5).next().value);
  function* f6(a = () => eval("'use strict'; x"), x) { return a() }
  assertEquals(4, f6(() => 4, 5).next().value);

  function* f11(a = x, x = 2) { return a }
  assertThrows(() => f11(), ReferenceError);
  assertThrows(() => f11(undefined), ReferenceError);
  assertThrows(() => f11(undefined, 4), ReferenceError);
  assertEquals(4, f1(4, 5).next().value);
  function* f12(a = eval("x"), x = 2) { return a }
  assertThrows(() => f12(), ReferenceError);
  assertThrows(() => f12(undefined), ReferenceError);
  assertThrows(() => f12(undefined, 4), ReferenceError);
  assertEquals(4, f12(4, 5).next().value);
  function* f13(a = eval("'use strict'; x"), x = 2) { return a }
  assertThrows(() => f13(), ReferenceError);
  assertThrows(() => f13(undefined), ReferenceError);
  assertThrows(() => f13(undefined, 4), ReferenceError);
  assertEquals(4, f13(4, 5).next().value);

  function* f21(x = function() { return a }, ...a) { return x()[0] }
  assertEquals(4, f21(undefined, 4).next().value);
  function* f22(x = () => a, ...a) { return x()[0] }
  assertEquals(4, f22(undefined, 4).next().value);
  function* f23(x = () => eval("a"), ...a) { return x()[0] }
  assertEquals(4, f23(undefined, 4).next().value);
  function* f24(x = () => {'use strict'; return eval("a") }, ...a) {
    return x()[0]
  }
  assertEquals(4, f24(undefined, 4).next().value);
  function* f25(x = () => eval("'use strict'; a"), ...a) { return x()[0] }
  assertEquals(4, f25(undefined, 4).next().value);
})();

(function TestParameterTDZStrict() {
  "use strict";

  function* f1(a = eval("x"), x) { return a }
  assertThrows(() => f1(undefined, 4), ReferenceError);
  assertEquals(4, f1(4, 5).next().value);
  function* f2(a = () => eval("x"), x) { return a() }
  assertEquals(4, f2(() => 4, 5).next().value);

  function* f11(a = eval("x"), x = 2) { return a }
  assertThrows(() => f11(), ReferenceError);
  assertThrows(() => f11(undefined), ReferenceError);
  assertThrows(() => f11(undefined, 4), ReferenceError);
  assertEquals(4, f11(4, 5).next().value);

  function* f21(x = () => eval("a"), ...a) { return x()[0] }
  assertEquals(4, f21(undefined, 4).next().value);
})();

(function TestArgumentsForNonSimpleParameters() {
  function* f1(x = 900) { arguments[0] = 1; return x }
  assertEquals(9, f1(9).next().value);
  assertEquals(900, f1().next().value);
  function* f2(x = 1001) { x = 2; return arguments[0] }
  assertEquals(10, f2(10).next().value);
  assertEquals(undefined, f2().next().value);
}());


(function TestFunctionLength() {
   assertEquals(0, (function*(x = 1) {}).length);
   assertEquals(0, (function*(x = 1, ...a) {}).length);
   assertEquals(1, (function*(x, y = 1) {}).length);
   assertEquals(1, (function*(x, y = 1, ...a) {}).length);
   assertEquals(2, (function*(x, y, z = 1) {}).length);
   assertEquals(2, (function*(x, y, z = 1, ...a) {}).length);
   assertEquals(1, (function*(x, y = 1, z) {}).length);
   assertEquals(1, (function*(x, y = 1, z, ...a) {}).length);
   assertEquals(1, (function*(x, y = 1, z, v = 2) {}).length);
   assertEquals(1, (function*(x, y = 1, z, v = 2, ...a) {}).length);
})();

(function TestDirectiveThrows() {
  "use strict";

  assertThrows("(function*(x=1){'use strict';})", SyntaxError);
  assertThrows("(function*(a, x=1){'use strict';})", SyntaxError);
  assertThrows("(function*({x}){'use strict';})", SyntaxError);
})();
