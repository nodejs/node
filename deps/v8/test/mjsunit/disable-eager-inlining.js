// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-non-eager-inlining --no-turbofan
// Flags: --no-optimize-maglev-optimizes-to-turbofan

(function TestDirectOptimizationStillWorks() {
  function fun(x) {
    return x + 1;
  }
  %DisableEagerInlining(fun);
  %PrepareFunctionForOptimization(fun);
  assertEquals(43, fun(42));
  assertEquals(43, fun(42));
  %OptimizeMaglevOnNextCall(fun);
  assertEquals(43, fun(42));
  assertMaglevved(fun);
})();

(function TestDisablingOptimizationStillWorks() {
  function fun(x) {
    return x + 1;
  }
  %DisableEagerInlining(fun);
  %NeverOptimizeFunction(fun);
  %PrepareFunctionForOptimization(fun);
  assertEquals(43, fun(42));
  assertEquals(43, fun(42));
  %OptimizeMaglevOnNextCall(fun);
  assertEquals(43, fun(42));
  assertUnoptimized(fun);
})();

(function TestNonEagerInliningWithDependency() {
  const proto = { x: 1 };
  const obj = Object.create(proto);

  function callee() {
    return obj.x;
  }
  %DisableEagerInlining(callee);

  function caller() {
    return callee() + 10;
  }

  %PrepareFunctionForOptimization(callee);
  %PrepareFunctionForOptimization(caller);
  assertEquals(11, caller());
  assertEquals(11, caller());

  %OptimizeMaglevOnNextCall(caller);
  assertEquals(11, caller());
  assertMaglevved(caller);

  // Invalidate prototype stability dependency.
  proto.y = 2;

  // callee was non-eagerly inlined into caller, so caller must deoptimize.
  assertUnoptimized(caller);
  assertEquals(11, caller());
})();
