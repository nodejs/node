// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const A = class A extends Array {
  constructor() {
    super();
    this.y = 1;
  }
}

function foo(x) {
  var a = new A();
  if (x) return a.y;
}

assertEquals(undefined, foo(false));
assertEquals(undefined, foo(false));
%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo(false));
assertEquals(1, foo(true));
