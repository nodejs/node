// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --turboshaft-enable-debug-features

var expect_interpreted = true;

function C() {
  this.a = 1;
  assertEquals(expect_interpreted, %IsBeingInterpreted());
  if (!%IsDictPropertyConstTrackingEnabled()) {
    // TODO(v8:11457) If v8_dict_property_const_tracking is enabled, then the
    // prototype of |this| in D() is a dictionary mode object, and we cannot
    // inline the storing of this.x, yet.
    %TurbofanStaticAssert(this.x == 42);
  }
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
var c = new C;
var d = new D;
var e = new E;
var f = new F;
var g = new G;

foo();
foo();
expect_interpreted = false;
%OptimizeFunctionOnNextCall(foo);
foo();
