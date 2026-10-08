// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class Base {
  constructor(x) {
    this.a = 1;
  }
}

class Derived extends Base {
  constructor(x) {
    try {
      if (x) throw 1;
      super(x);
      throw 2;
    } catch (e) {
      return { caught: e };
    }
  }
}

function foo(x) {
  return new Derived(x);
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(Derived);
%PrepareFunctionForOptimization(Base);

for (let i = 0; i < 50; i++) {
  foo(i % 2 === 0);
}

%OptimizeMaglevOnNextCall(foo);
foo(false);
