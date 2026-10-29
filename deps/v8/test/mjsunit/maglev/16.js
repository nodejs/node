// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(x) {
  return x.a;
}

function Foo(a) {
  this.a = a;
}

function Goo(a) {
  this.a = a;
}

%PrepareFunctionForOptimization(f);

var o1 = new Foo(42);
var o2 = new Goo(4.2);

assertEquals(f(o1), 42);
assertEquals(f(o2), 4.2);

%OptimizeMaglevOnNextCall(f);

assertEquals(f(o1), 42);
assertEquals(f(o2), 4.2);
