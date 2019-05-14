// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function regressionCaseOne() {
  var c;
  for (let [a, b = c = function() { return a + b }] of [[0]]) {
    function f() { return a };
  }
  c();
})();

(function testForInFunction() {
  for (const {length: a, b = function() { return a, b }} in {foo: 42}) {
    assertSame(b, (function() { return b() })());
  }
})();

(function testForOfFunction() {
  for (const [a, b = function() { return a, b }] of [[42]]) {
    assertSame(b, (function() { return b() })());
  }
})();

(function testForInVariableProxy() {
  for (const {length: a, b = a} in {foo: 42}) {
    assertEquals(3, a);
    assertEquals(a, b);
  }
})();

(function testForOfVariableProxy() {
  for (const [a, b = a] of [[42]]) {
    assertEquals(42, a);
    assertEquals(a, b);
  }
})();

(function testClassLiteral() {
  for (let { a, b = class c { static f() { return a, b } } } of [{}]) {
    assertSame(b, (function() { return b.f() })());
  }
})();

// Methods in class literals remain inside the
// class scope after scope reparenting.
(function testClassLiteralMethod() {
  for (let { a, b = class c { m() { return c } } } of [{}]) {
    assertSame(b, (function() { return (new b).m() })());
  }
})();

// Function literals in computed class names remain inside the
// class scope after scope reparenting.
(function testClassLiteralComputedName() {
  let d;
  for (let { a, b = class c { [d = function() { return c }]() { } } } of [{}]) {
    assertSame(b, (function() { return b, d() })());
  }
})();

// Function literals in class extends expressions names remain inside the
// class scope after scope reparenting.
(function testClassLiteralComputedName() {
  let d;
  for (let { a, b = class c extends (d = function() { return c }, Object) { } } of [{}]) {
    assertSame(b, (function() { return b, d() })());
  }
})();
