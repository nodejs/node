// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-specialize-for-script-context --turbofan
// Flags: --allow-natives-syntax

const scriptConst = 42;

function createClosure() {
  return function foo() {
    %TurbofanStaticAssert(scriptConst == 42);
    return scriptConst;
  };
}

// Create the closure twice to avoid function context specialization.
const f1 = createClosure();
const f2 = createClosure();

%PrepareFunctionForOptimization(f1);
assertEquals(42, f1());
%OptimizeFunctionOnNextCall(f1);
assertEquals(42, f1());
assertOptimized(f1);

%PrepareFunctionForOptimization(f2);
assertEquals(42, f2());
%OptimizeFunctionOnNextCall(f2);
assertEquals(42, f2());
assertOptimized(f2);

function createClosure2() {
  return function bar() {
    %TurbofanStaticAssert(scriptConst == 42);
    return scriptConst;
  };
}

// Create and optimize the first closure (with function context specialization),
// then create and optimize a second closure (transitioning from
// one_closure_cell to many_closures_cell and recompiling without function
// context specialization).
const g1 = createClosure2();
%PrepareFunctionForOptimization(g1);
assertEquals(42, g1());
%OptimizeFunctionOnNextCall(g1);
assertEquals(42, g1());
assertOptimized(g1);

const g2 = createClosure2();
%PrepareFunctionForOptimization(g2);
assertEquals(42, g2());
%OptimizeFunctionOnNextCall(g2);
assertEquals(42, g2());
assertOptimized(g2);
assertEquals(42, g1());
assertOptimized(g1);
