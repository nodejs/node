// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

var expect_interpreted = true;

function C() {
  this.a = 1;
  assertEquals(expect_interpreted, %IsBeingInterpreted());
  %TurbofanStaticAssert(this.x == 42);
};

function D() {
  this.x = 42;
  C.call(this);
};

function E() {
  D.call(this);
}

function F() {
  E.call(this);
};

function G() {
  F.call(this);
};

function foo() {
  new D;
}

%PrepareFunctionForOptimization(C);
%PrepareFunctionForOptimization(D);
%PrepareFunctionForOptimization(E);
%PrepareFunctionForOptimization(F);
%PrepareFunctionForOptimization(G);
%PrepareFunctionForOptimization(foo);

// Make 'this.x' access in C megamorhpic.
new C;
new D;
new E;
new F;
new G;

foo();
foo();
%OptimizeFunctionOnNextCall(foo);
expect_interpreted = false;
foo();
