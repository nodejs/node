// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class A {
  constructor() {
    return globalThis;
  }
}

class B extends A {
  field = 'abc';
  constructor() {
    super();
  }
}

function f() {
  return new B();
}

%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
f();

function g() {
  class C extends A {
    #privateField = 1;
    constructor() {
      super();
    }
  }
  new C();
}

%PrepareFunctionForOptimization(g);
g();
g();
%OptimizeFunctionOnNextCall(g);
g();
g();
