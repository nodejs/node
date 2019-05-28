// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Foo(a, b) {
  var bname = "b";
  this["a"] = a;
  this[bname] = b;
  this.x = this.a + this.b;
}

var f1 = new Foo(3, 4);
assertEquals(7, f1.x);

// SMIs
for (var i = 0; i < 6; i++) {
  var f = new Foo(i, i + 2);
  assertEquals(i + i + 2, f.x);
}

// derbles
for (var i = 0.25; i < 6.25; i++) {
  var f = new Foo(i, i + 2);
  assertEquals(i + i + 2, f.x);
}

// stirngs
for (var i = 0; i < 6; i++) {
  var f = new Foo(i + "", (i + 2) + "");
  assertEquals((i + "") + ((i + 2) + ""), f.x);
}


{
  function Global(i) { this.bla = i }
  Global(0);
  Global(1);
  %OptimizeFunctionOnNextCall(Global);
  Global(2);
  assertEquals(bla, 2);
}


{
  function access(obj) { obj.bla = 42 }
  access({a: 0});
  access({b: 0});
  access({c: 0});
  access({d: 0});
  access({e: 0});
  var global = this;
  function foo() { access(global) };
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
}
