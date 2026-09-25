// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function setA(o, val) {
  o.a = val;
}

function Foo() {
  this.a = 0;
}

var foo = new Foo();

%PrepareFunctionForOptimization(setA);

setA(foo, 1);
assertEquals(foo.a, 1);
setA(foo, 2);
assertEquals(foo.a, 2);

%OptimizeMaglevOnNextCall(setA);

setA(foo, 42);
assertEquals(foo.a, 42);
