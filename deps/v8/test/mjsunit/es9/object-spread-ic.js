// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testDoubleElements() {
  function f(src) { return {...src}; }
  var src = [1.5];
  src[0] = 1;

  // Uninitialized
  assertEquals({ 0: 1 }, f(src));

  src[0] = 1.3;

  // Monomorphic
  assertEquals({ 0: 1.3 }, f(src));
})();

(function testInObjectProperties() {
  function f(src) { return {...src}; }
  function C() { this.foo = "foo"; }
  var src;
  for (var i = 0; i < 10; ++i) {
    src = new C();
  }

  // Uninitialized
  assertEquals({ foo: "foo" }, f(src));

  // Monomorphic
  assertEquals({ foo: "foo" }, f(src));
})();

(function testInObjectProperties2() {
  function f(src) { return {...src}; }
  function C() {
    this.foo = "foo";
    this.p0 = "0";
    this.p1 = "1";
    this.p2 = "2";
    this.p3 = "3";
  }
  var src;
  for (var i = 0; i < 10; ++i) {
    src = new C();
  }

  // Uninitialized
  assertEquals({ foo: "foo", p0: "0", p1: "1", p2: "2", p3: "3" }, f(src));

  // Monomorphic
  assertEquals({ foo: "foo", p0: "0", p1: "1", p2: "2", p3: "3" }, f(src));
})();

(function testPolymorphicToMegamorphic() {
  function f(src) { return {...src}; }
  function C1() {
    this.foo = "foo";
    this.p0 = "0";
    this.p1 = "1";
    this.p2 = "2";
    this.p3 = "3";
  }
  function C2() {
    this.p0 = "0";
    this.p1 = "1";
    this[0] = 0;
  }
  function C3() {
    this.x = 774;
    this.y = 663;
    this.rgb = 0xFF00FF;
  }
  function C4() {
    this.qqq = {};
    this.v_1 = [];
    this.name = "C4";
    this.constructor = C4;
  }

  // Uninitialized
  assertEquals({ foo: "foo", p0: "0", p1: "1", p2: "2", p3: "3" }, f(new C1()));

  // Monomorphic
  assertEquals({ foo: "foo", p0: "0", p1: "1", p2: "2", p3: "3" }, f(new C1()));

  // Polymorphic (2)
  assertEquals({ 0: 0, p0: "0", p1: "1" }, f(new C2()));
  assertEquals({ 0: 0, p0: "0", p1: "1" }, f(new C2()));

  // Polymorphic (3)
  assertEquals({ x: 774, y: 663, rgb: 0xFF00FF }, f(new C3()));
  assertEquals({ x: 774, y: 663, rgb: 0xFF00FF }, f(new C3()));

  // Polymorphic (4)
  assertEquals({ qqq: {}, v_1: [], name: "C4", constructor: C4 }, f(new C4()));
  assertEquals({ qqq: {}, v_1: [], name: "C4", constructor: C4 }, f(new C4()));

  // Megamorphic
  assertEquals({ boop: 1 }, f({ boop: 1 }));
})();
