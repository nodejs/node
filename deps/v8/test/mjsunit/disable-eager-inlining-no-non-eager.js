// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan
// Flags: --no-maglev-non-eager-inlining
// Flags: --no-optimize-maglev-optimizes-to-turbofan

(function TestEagerInliningBaseline() {
  const proto = { x: 1 };
  const obj = Object.create(proto);

  function callee() {
    return obj.x;
  }

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

  // callee was eagerly inlined into caller, so caller must deoptimize.
  assertUnoptimized(caller);
  assertEquals(11, caller());
})();

(function TestEagerInliningDisabled() {
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

  // Eager inlining was disabled and non-eager inlining is disabled, so callee
  // was not inlined. caller must remain Maglevved.
  assertMaglevved(caller);
  assertEquals(11, caller());
})();
