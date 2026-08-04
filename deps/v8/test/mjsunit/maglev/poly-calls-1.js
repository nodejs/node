// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev
// Flags: --maglev-poly-calls

function foo(o) {
  return o.foo();
}
%PrepareFunctionForOptimization(foo);

function Thing1() {
  this.a = 0;
}

Thing1.prototype.foo = function() {
  return 0x100/2; // 128 aka Smi 0x100
};
%PrepareFunctionForOptimization(Thing1.prototype.foo);

function Thing2() {
  this.b = 0;
}

Thing2.prototype.foo = function() {
  return 0x200/2; // 256 aka Smi 0x200
};
%PrepareFunctionForOptimization(Thing2.prototype.foo);

let o1  = new Thing1();
let o2  = new Thing2();

foo(o1);
foo(o2);

%OptimizeMaglevOnNextCall(foo);

assertEquals(128, foo(o1));
assertEquals(256, foo(o2));
assertTrue(isMaglevved(foo));

let somethingElse = {foo: () => {return -8}};
assertEquals(-8, foo(somethingElse));
assertFalse(isMaglevved(foo));
