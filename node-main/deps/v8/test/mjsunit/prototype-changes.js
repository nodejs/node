// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function A() {
  this.a = "a";
}
var a = new A();

function B() {
  this.b = "b";
}
B.prototype = a;

function C() {
  this.c = "c";
}
C.prototype = new B();

var c = new C();

function f(expected) {
  var result = c.z;
  assertEquals(expected, result);
};
%PrepareFunctionForOptimization(f);
f(undefined);
f(undefined);
%OptimizeFunctionOnNextCall(f);
f(undefined);
a.z = "z";
f("z");
f("z");

// Test updating .__proto__ pointers.
var p1 = {foo: 1.5};
var p2 = {};
p2.__proto__ = p1;
var p3 = {};
p3.__proto__ = p2;
var o = {};
o.__proto__ = p3;

for (var i = 0; i < 2; i++) o.foo;  // Force registration.

var p1a = {foo: 1.7};
p2.__proto__ = p1a;

function g(o, expected) {
  var result = o.foo;
  assertEquals(expected, result);
}

g(o, 1.7);
g(o, 1.7);
g(o, 1.7);
Object.defineProperty(p1a, 'foo', {
  get: function() {
    return 'foo';
  }
});
g(o, "foo");
