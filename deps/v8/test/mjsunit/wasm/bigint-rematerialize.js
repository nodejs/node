// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan --turbo-inline-js-wasm-calls

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

builder
    .addFunction("f", kSig_l_v) // () -> i64
    .addBody([
      kExprI64Const, 0,
      kExprI64Const, 1,
      kExprI64Sub, // -1
    ])
    .exportFunc();

let module = builder.instantiate();

function f(x) {
  let y = module.exports.f();
  try {
    return x + y;
  } catch(_) {
    return y;
  }
}

%PrepareFunctionForOptimization(f);
assertEquals(0n, f(1n));
assertEquals(1n, f(2n));
%OptimizeFunctionOnNextCall(f);
assertEquals(0n, f(1n));
assertOptimized(f);
// After optimization, the result of the js wasm call is stored in word64 and
// passed to StateValues without conversion. Rematerialization will happen
// in deoptimizer.
assertEquals(-1n, f(0));
if (%Is64Bit()) {
  assertUnoptimized(f);
}
