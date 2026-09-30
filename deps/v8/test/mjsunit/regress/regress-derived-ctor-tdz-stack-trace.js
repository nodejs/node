// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class Base {}

class DerivedBeforeSuper extends Base {
  constructor() {
    const stack = new Error().stack;
    assertTrue(typeof stack === 'string' && stack.length > 0);
    super();
  }
}

class DerivedCapturedThis extends Base {
  constructor() {
    const capture = () => this;
    const stack_before = new Error().stack;
    assertTrue(typeof stack_before === 'string' && stack_before.length > 0);
    super();
    const stack_after = new Error().stack;
    assertTrue(typeof stack_after === 'string' && stack_after.length > 0);
    assertSame(this, capture());
  }
}

function test() {
  new DerivedBeforeSuper();
  new DerivedCapturedThis();
}

%PrepareFunctionForOptimization(DerivedBeforeSuper);
%PrepareFunctionForOptimization(DerivedCapturedThis);
%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeFunctionOnNextCall(DerivedBeforeSuper);
%OptimizeFunctionOnNextCall(DerivedCapturedThis);
%OptimizeFunctionOnNextCall(test);
test();
