// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-fp16
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
const dummy_index = builder.addImport("env", "dummy", kSig_v_v);

builder.addFunction("test_spill", kSig_f_f)
  .addLocals(kWasmS128, 1)
  .addBody([
    ...wasmF32Const(0.0),
    kSimdPrefix, ...kExprF16x8Splat,
    kExprLocalSet, 1,
    kExprCallFunction, dummy_index,
    kExprLocalGet, 1,
    kExprLocalGet, 0,
    kSimdPrefix, ...kExprF16x8ReplaceLane, 0,
    kSimdPrefix, ...kExprF16x8ExtractLane, 0,
  ])
  .exportFunc();

const instance = builder.instantiate({env: {dummy: () => {}}});
instance.exports.test_spill(1.5);
