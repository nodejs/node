// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev
// Flags: --maglev-poly-calls

let outsideObject = {a: 10};
// Make both the object and the field non-const, so we don't embed them in the
// optimized code.
outsideObject = {a: 11};
outsideObject.a = 20;

function foo(o) {
  // This first loads o.foo, then loads outsideObject.a., then calls o.foo.
  return o.foo(outsideObject.a);
}
%PrepareFunctionForOptimization(foo);

function Thing1() {
  this.a = 0;
}

Thing1.prototype.foo = function(x) {
  return 128 + x;
};
%PrepareFunctionForOptimization(Thing1.prototype.foo);

function Thing2() {
  this.b = 0;
}

Thing2.prototype.foo = function(x) {
  return 256 + x;
};
%PrepareFunctionForOptimization(Thing2.prototype.foo);

let o1  = new Thing1();
let o2  = new Thing2();

foo(o1);
foo(o2);

%OptimizeMaglevOnNextCall(foo);

assertEquals(128 + 20, foo(o1));
assertEquals(256 + 20, foo(o2));

outsideObject.a = 10;
assertEquals(128 + 10, foo(o1));
assertEquals(256 + 10, foo(o2));
assertTrue(isMaglevved(foo));

// Change the map of `outsideObject`; this will also deopt.
outsideObject = {aa: 0, a: 0};

assertEquals(128, foo(o1));
assertFalse(isMaglevved(foo));
