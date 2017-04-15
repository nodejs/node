// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Dictionary object in the prototype chain.
(function() {
  function A() {
    this.z = "a";
  }
  var a = new A();

  function B() {
    this.b = "b";
  }
  B.prototype = a;
  var b = new B();

  // Ensure b stays slow.
  for (var i = 0; i < 1200; i++) {
    b["b"+i] = 0;
  }
  assertFalse(%HasFastProperties(b));

  function C() {
    this.c = "c";
  }
  C.prototype = b;
  var c = new C();

  function f(expected) {
    assertFalse(%HasFastProperties(b));
    var result = c.z;
    assertEquals(expected, result);
  }
  f("a");
  f("a");
  f("a");
  %OptimizeFunctionOnNextCall(f);
  f("a");

  a.z = "z";
  f("z");
  f("z");
  f("z");

  b.z = "bz";
  f("bz");
  f("bz");
  f("bz");
})();


// Global object in the prototype chain.
(function() {
  var global = this;

  function A() {
    this.z = "a";
  }
  A.prototype = global.__proto__;
  var a = new A();

  global.__proto__ = a;

  function C() {
    this.c = "c";
  }
  C.prototype = global;
  var c = new C();

  function f(expected) {
    var result = c.z;
    assertEquals(expected, result);
  }
  f("a");
  f("a");
  f("a");
  %OptimizeFunctionOnNextCall(f);
  f("a");

  a.z = "z";
  f("z");
  f("z");
  f("z");

  global.z = "bz";
  f("bz");
  f("bz");
  f("bz");
})();
