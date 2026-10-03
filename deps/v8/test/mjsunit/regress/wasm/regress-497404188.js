// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev

// Regression test for a type confusion in the deoptimizer caused by missing
// signature comparison in FrameStateFunctionInfo::operator==.
// Two wasm functions with different return types (externref vs i64) but
// identical parameter signatures can have their JSToWasmBuiltinContinuation
// frame states merged by CSE. On lazy deopt, the deoptimizer then uses the
// wrong signature to materialize the result, reading a tagged reference as
// an untagged i64 or vice versa.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function makeInstance(callback) {
  const builder = new WasmModuleBuilder();
  const callback_index = builder.addImport('env', 'callback', kSig_v_v);

  // Mutable Wasm globals to hold the return values for the two functions. The
  // callback can modify these to trigger deopt at the right moment and test
  // that the correct type is materialized after deopt.
  const g_ref =
      builder.addGlobal(kWasmExternRef, true, false).exportAs('g_ref');
  const g_i64 = builder.addGlobal(kWasmI64, true, false).exportAs('g_i64');

  // Returns an externref global after calling the callback (which may trigger
  // deopt).
  builder.addFunction('return_ref', kSig_r_v)
      .addBody([
        kExprCallFunction, callback_index,
        kExprGlobalGet, g_ref.index,
      ])
      .exportFunc();

  // Returns an i64 global after calling the callback (which may trigger deopt).
  builder.addFunction('return_i64', kSig_l_v)
      .addBody([
        kExprCallFunction, callback_index,
        kExprGlobalGet, g_i64.index,
      ])
      .exportFunc();

  return builder.instantiate({env: {callback}}).exports;
}

(function testNoTypeConfusionOnLazyDeopt() {
  let arm_deopt = false;

  function ProtoForI64() {}
  function ProtoForRef() {}

  const exports_ = makeInstance(() => {
    // Trigger deopt by changing the prototype after optimization.
    if (arm_deopt) {
      ProtoForRef.prototype.deopt_marker = 1;
    }
  });

  // Install wasm getters with different return types on different prototypes.
  Object.defineProperty(
      ProtoForI64.prototype, 'x', {get: exports_.return_i64});
  Object.defineProperty(
      ProtoForRef.prototype, 'x', {get: exports_.return_ref});

  function foo(o) {
    return o.x;
  }

  const obj_i64 = new ProtoForI64();
  const obj_ref = new ProtoForRef();

  const sentinel = {tag: 'sentinel'};
  exports_.g_ref.value = sentinel;
  exports_.g_i64.value = 42n;

  // Train the function with both receiver types.
  %PrepareFunctionForOptimization(foo);
  for (let i = 0; i < 20; ++i) {
    foo(obj_i64);
    foo(obj_ref);
  }

  // Optimize and run once without deopt.
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(42n, foo(obj_i64));

  arm_deopt = true;
  const result = foo(obj_ref);
  assertEquals(sentinel, result);
})();
