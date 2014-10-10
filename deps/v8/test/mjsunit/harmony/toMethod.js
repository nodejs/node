// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-classes --allow-natives-syntax


(function TestSingleClass() {
  function f(x) {
    var a = [0, 1, 2]
    return a[x];
  }

  function ClassD() { }

  assertEquals(1, f(1));
  var g = f.toMethod(ClassD.prototype);
  assertEquals(1, g(1));
  assertEquals(undefined, f[%HomeObjectSymbol()]);
  assertEquals(ClassD.prototype, g[%HomeObjectSymbol()]);
}());


(function TestClassHierarchy() {
  function f(x) {
    return function g(y)  { x++; return x + y; };
  }

  function Base() {}
  function Derived() { }
  Derived.prototype = Object.create(Base.prototype);

  var q = f(0);
  assertEquals(2, q(1));
  assertEquals(3, q(1));
  var g = q.toMethod(Derived.prototype);
  assertFalse(g === q);
  assertEquals(4, g(1));
  assertEquals(5, q(1));
}());


(function TestErrorCases() {
  var sFun = Function.prototype.toMethod;
  assertThrows(function() { sFun.call({}); }, TypeError);
  assertThrows(function() { sFun.call({}, {}); }, TypeError);
  function f(){};
  assertThrows(function() { f.toMethod(1); }, TypeError);
}());


(function TestPrototypeChain() {
  var o = {};
  var o1 = {};
  function f() { }

  function g() { }

  var fMeth = f.toMethod(o);
  assertEquals(o, fMeth[%HomeObjectSymbol()]);
  g.__proto__ = fMeth;
  assertEquals(undefined, g[%HomeObjectSymbol()]);
  var gMeth = g.toMethod(o1);
  assertEquals(fMeth, gMeth.__proto__);
  assertEquals(o, fMeth[%HomeObjectSymbol()]);
  assertEquals(o1, gMeth[%HomeObjectSymbol()]);
}());


(function TestBoundFunction() {
  var o = {};
  var p = {};


  function f(x, y, z, w) {
    assertEquals(o, this);
    assertEquals(1, x);
    assertEquals(2, y);
    assertEquals(3, z);
    assertEquals(4, w);
    return x+y+z+w;
  }

  var fBound = f.bind(o, 1, 2, 3);
  var fMeth = fBound.toMethod(p);
  assertEquals(10, fMeth(4));
  assertEquals(10, fMeth.call(p, 4));
  var fBound1 = fBound.bind(o, 4);
  assertEquals(10, fBound1());
  var fMethBound = fMeth.bind(o, 4);
  assertEquals(10, fMethBound());
}());

(function TestOptimized() {
  function f(o) {
    return o.x;
  }
  var o = {x : 15};
  assertEquals(15, f(o));
  assertEquals(15, f(o));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(15, f(o));
  var g = f.toMethod({});
  var o1 = {y : 1024, x : "abc"};
  assertEquals("abc", f(o1));
  assertEquals("abc", g(o1));
} ());

(function TestExtensibility() {
  function f() {}
  Object.preventExtensions(f);
  assertFalse(Object.isExtensible(f));
  var m = f.toMethod({});
  assertTrue(Object.isExtensible(m));
}());
