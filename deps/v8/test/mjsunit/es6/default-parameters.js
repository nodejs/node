// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


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


(function TestParameterScopingSloppy() {
  var x = 1;

  function f1(a = x) { var x = 2; return a; }
  assertEquals(1, f1());
  function f2(a = x) { function x() {}; return a; }
  assertEquals(1, f2());
  function f3(a = eval("x")) { var x; return a; }
  assertEquals(1, f3());
  function f31(a = eval("'use strict'; x")) { var x; return a; }
  assertEquals(1, f31());
  function f4(a = function() { return x }) { var x; return a(); }
  assertEquals(1, f4());
  function f5(a = () => x) { var x; return a(); }
  assertEquals(1, f5());
  function f6(a = () => eval("x")) { var x; return a(); }
  assertEquals(1, f6());
  function f61(a = () => { 'use strict'; return eval("x") }) { var x; return a(); }
  assertEquals(1, f61());
  function f62(a = () => eval("'use strict'; x")) { var x; return a(); }
  assertEquals(1, f62());

  var g1 = (a = x) => { var x = 2; return a; };
  assertEquals(1, g1());
  var g2 = (a = x) => { function x() {}; return a; };
  assertEquals(1, g2());
  var g3 = (a = eval("x")) => { var x; return a; };
  assertEquals(1, g3());
  var g31 = (a = eval("'use strict'; x")) => { var x; return a; };
  assertEquals(1, g31());
  var g4 = (a = function() { return x }) => { var x; return a(); };
  assertEquals(1, g4());
  var g5 = (a = () => x) => { var x; return a(); };
  assertEquals(1, g5());
  var g6 = (a = () => eval("x")) => { var x; return a(); };
  assertEquals(1, g6());
  var g61 = (a = () => { 'use strict'; return eval("x") }) => { var x; return a(); };
  assertEquals(1, g61());
  var g62 = (a = () => eval("'use strict'; x")) => { var x; return a(); };
  assertEquals(1, g62());

  var f11 = function f(x = f) { var f; return x; }
  assertSame(f11, f11());
  var f12 = function f(x = f) { function f() {}; return x; }
  assertSame(f12, f12());
  var f13 = function f(f = 7, x = f) { return x; }
  assertSame(7, f13());

  var o1 = {f: function(x = this) { return x; }};
  assertSame(o1, o1.f());
  assertSame(1, o1.f(1));
})();

(function TestParameterScopingStrict() {
  "use strict";
  var x = 1;

  function f1(a = x) { let x = 2; return a; }
  assertEquals(1, f1());
  function f2(a = x) { const x = 2; return a; }
  assertEquals(1, f2());
  function f3(a = x) { function x() {}; return a; }
  assertEquals(1, f3());
  function f4(a = eval("x")) { var x; return a; }
  assertEquals(1, f4());
  function f5(a = () => eval("x")) { var x; return a(); }
  assertEquals(1, f5());

  var g1 = (a = x) => { let x = 2; return a; };
  assertEquals(1, g1());
  var g2 = (a = x) => { const x = 2; return a; };
  assertEquals(1, g2());
  var g3 = (a = x) => { function x() {}; return a; };
  assertEquals(1, g3());
  var g4 = (a = eval("x")) => { var x; return a; };
  assertEquals(1, g4());
  var g5 = (a = () => eval("x")) => { var x; return a(); };
  assertEquals(1, g5());

  var f11 = function f(x = f) { let f; return x; }
  assertSame(f11, f11());
  var f12 = function f(x = f) { const f = 0; return x; }
  assertSame(f12, f12());
  var f13 = function f(x = f) { function f() {}; return x; }
  assertSame(f13, f13());
})();

(function TestSloppyEvalScoping() {
  var x = 1;

  function f1(y = eval("var x = 2")) { with ({}) { return x; } }
  assertEquals(2, f1());
  function f2(y = eval("var x = 2"), z = x) { return z; }
  assertEquals(2, f2());
  assertEquals(1, f2(0));
  function f3(y = eval("var x = 2"), z = eval("x")) { return z; }
  assertEquals(2, f3());
  assertEquals(1, f3(0));
  function f8(y = (eval("var x = 2"), x)) { return y; }
  assertEquals(2, f8());
  assertEquals(0, f8(0));

  function f11(z = eval("var y = 2")) { return y; }
  assertEquals(2, f11());
  function f12(z = eval("var y = 2"), b = y) { return b; }
  assertEquals(2, f12());
  function f13(z = eval("var y = 2"), b = eval("y")) { return b; }
  assertEquals(2, f13());

  function f21(f = () => x) { eval("var x = 2"); return f() }
  assertEquals(1, f21());
  assertEquals(3, f21(() => 3));
  function f22(f = () => eval("x")) { eval("var x = 2"); return f() }
  assertEquals(1, f22());
  assertEquals(3, f22(() => 3));

  var g1 = (y = eval("var x = 2")) => { with ({}) { return x; } };
  assertEquals(2, g1());
  var g2 = (y = eval("var x = 2"), z = x) => { return z; };
  assertEquals(2, g2());
  assertEquals(1, g2(0));
  var g3 = (y = eval("var x = 2"), z = eval("x")) => { return z; };
  assertEquals(2, g3());
  assertEquals(1, g3(0));
  var g8 = (y = (eval("var x = 2"), x)) => { return y; };
  assertEquals(2, g8());
  assertEquals(0, g8(0));

  var g11 = (z = eval("var y = 2")) => { return y; };
  assertEquals(2, g11());
  var g12 = (z = eval("var y = 2"), b = y) => { return b; };
  assertEquals(2, g12());
  var g13 = (z = eval("var y = 2"), b = eval("y")) => { return b; };
  assertEquals(2, g13());

  var g21 = (f = () => x) => { eval("var x = 2"); return f() };
  assertEquals(1, g21());
  assertEquals(3, g21(() => 3));
  var g22 = (f = () => eval("x")) => { eval("var x = 2"); return f() };
  assertEquals(1, g22());
  assertEquals(3, g22(() => 3));
})();


(function TestStrictEvalScoping() {
  'use strict';
  var x = 1;

  function f1(y = eval("var x = 2")) { return x; }
  assertEquals(1, f1());
  function f2(y = eval("var x = 2"), z = x) { return z; }
  assertEquals(1, f2());
  assertEquals(1, f2(0));
  function f3(y = eval("var x = 2"), z = eval("x")) { return z; }
  assertEquals(1, f3());
  assertEquals(1, f3(0));
  function f8(y = (eval("var x = 2"), x)) { return y; }
  assertEquals(1, f8());
  assertEquals(0, f8(0));

  function f11(z = eval("var y = 2")) { return y; }
  assertThrows(f11, ReferenceError);
  function f12(z = eval("var y = 2"), b = y) {}
  assertThrows(f12, ReferenceError);
  function f13(z = eval("var y = 2"), b = eval("y")) {}
  assertThrows(f13, ReferenceError);

  function f21(f = () => x) { eval("var x = 2"); return f() }
  assertEquals(1, f21());
  assertEquals(3, f21(() => 3));
  function f22(f = () => eval("x")) { eval("var x = 2"); return f() }
  assertEquals(1, f22());
  assertEquals(3, f22(() => 3));
})();

(function TestParameterTDZSloppy() {
  function f1(a = x, x) { return a }
  assertThrows(() => f1(undefined, 4), ReferenceError);
  assertEquals(4, f1(4, 5));
  function f2(a = eval("x"), x) { return a }
  assertThrows(() => f2(undefined, 4), ReferenceError);
  assertEquals(4, f2(4, 5));
  function f3(a = eval("'use strict'; x"), x) { return a }
  assertThrows(() => f3(undefined, 4), ReferenceError);
  assertEquals(4, f3(4, 5));
  function f4(a = () => x, x) { return a() }
  assertEquals(4, f4(() => 4, 5));
  function f5(a = () => eval("x"), x) { return a() }
  assertEquals(4, f5(() => 4, 5));
  function f6(a = () => eval("'use strict'; x"), x) { return a() }
  assertEquals(4, f6(() => 4, 5));

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
  function f13(a = eval("'use strict'; x"), x = 2) { return a }
  assertThrows(() => f13(), ReferenceError);
  assertThrows(() => f13(undefined), ReferenceError);
  assertThrows(() => f13(undefined, 4), ReferenceError);
  assertEquals(4, f13(4, 5));

  function f21(x = function() { return a }, ...a) { return x()[0] }
  assertEquals(4, f21(undefined, 4));
  function f22(x = () => a, ...a) { return x()[0] }
  assertEquals(4, f22(undefined, 4));
  function f23(x = () => eval("a"), ...a) { return x()[0] }
  assertEquals(4, f23(undefined, 4));
  function f24(x = () => {'use strict'; return eval("a") }, ...a) {
    return x()[0]
  }
  assertEquals(4, f24(undefined, 4));
  function f25(x = () => eval("'use strict'; a"), ...a) { return x()[0] }
  assertEquals(4, f25(undefined, 4));

  var g1 = (x = function() { return a }, ...a) => { return x()[0] };
  assertEquals(4, g1(undefined, 4));
  var g2 = (x = () => a, ...a) => { return x()[0] };
  assertEquals(4, g2(undefined, 4));
})();

(function TestParameterTDZStrict() {
  "use strict";

  function f1(a = eval("x"), x) { return a }
  assertThrows(() => f1(undefined, 4), ReferenceError);
  assertEquals(4, f1(4, 5));
  function f2(a = () => eval("x"), x) { return a() }
  assertEquals(4, f2(() => 4, 5));

  function f11(a = eval("x"), x = 2) { return a }
  assertThrows(() => f11(), ReferenceError);
  assertThrows(() => f11(undefined), ReferenceError);
  assertThrows(() => f11(undefined, 4), ReferenceError);
  assertEquals(4, f11(4, 5));

  function f21(x = () => eval("a"), ...a) { return x()[0] }
  assertEquals(4, f21(undefined, 4));
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
   assertEquals(0, (function(x = 1) {}).length);
   assertEquals(0, (function(x = 1, ...a) {}).length);
   assertEquals(1, (function(x, y = 1) {}).length);
   assertEquals(1, (function(x, y = 1, ...a) {}).length);
   assertEquals(2, (function(x, y, z = 1) {}).length);
   assertEquals(2, (function(x, y, z = 1, ...a) {}).length);
   assertEquals(1, (function(x, y = 1, z) {}).length);
   assertEquals(1, (function(x, y = 1, z, ...a) {}).length);
   assertEquals(1, (function(x, y = 1, z, v = 2) {}).length);
   assertEquals(1, (function(x, y = 1, z, v = 2, ...a) {}).length);
})();

(function TestDirectiveThrows() {
  "use strict";

  assertThrows("(function(x=1){'use strict';})", SyntaxError);
  assertThrows("(x=1) => {'use strict';}", SyntaxError);
  assertThrows("(class{foo(x=1) {'use strict';}});", SyntaxError);

  assertThrows("(function(a, x=1){'use strict';})", SyntaxError);
  assertThrows("(a, x=1) => {'use strict';}", SyntaxError);
  assertThrows("(class{foo(a, x=1) {'use strict';}});", SyntaxError);

  assertThrows("(function({x}){'use strict';})", SyntaxError);
  assertThrows("({x}) => {'use strict';}", SyntaxError);
  assertThrows("(class{foo({x}) {'use strict';}});", SyntaxError);
})();
