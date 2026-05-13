// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-validation --no-wasm-native-module-cache --allow-natives-syntax --wasm-deopt

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_i_i = makeSig([kWasmI32], [kWasmI32]);
let sig_idx = builder.addType(sig_i_i);

// 'compatible' is just a helper to call main initially.
let compatible = builder.addFunction("compatible", sig_idx)
  .addBody([kExprLocalGet, 0]).exportFunc();

// 'callee' will be inlined into 'main'.
// When main is serialized, 'callee' will be a kEagerFunction (if it was executed).
let callee = builder.addFunction("callee", makeSig([kWasmI32, kWasmFuncRef], [kWasmI32]))
  .addBody([
    kExprLocalGet, 1,
    kGCPrefix, kExprRefCast, sig_idx,
    kExprDrop,
    kExprLocalGet, 0
  ]).exportFunc();

// 'main' will be tiered up to Turbofan and then serialized.
builder.addFunction("main", makeSig([kWasmI32, kWasmFuncRef], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprCallFunction, callee.index
  ])
  .exportFunc();

// 'other' has an incompatible signature to trigger deopt in callee's ref.cast.
builder.addFunction("other", kSig_v_v).addBody([]).exportFunc();

let wasmBytes = builder.toBuffer();
let wasmModule = new WebAssembly.Module(wasmBytes);
let instance = new WebAssembly.Instance(wasmModule);

let f_main = instance.exports.main;
let f_compatible = instance.exports.compatible;

// 1. Execute main with valid ref to collect feedback and make callee eager.
f_main(42, f_compatible);

// 2. Tier up main. Should inline callee.
%WasmTierUpFunction(f_main);

// 3. Serialize.
let serialized = d8.wasm.serializeModule(wasmModule);

// 4. Deserialize with lazy validation.
let module2 = d8.wasm.deserializeModule(serialized, wasmBytes);
let instance2 = new WebAssembly.Instance(module2);

let f2_main = instance2.exports.main;
let f2_other = instance2.exports.other;

// 5. Trigger deopt in the inlined callee.
// This will call ExecuteLiftoffCompilation for 'callee'.
// Without the fix, this hits a DCHECK because 'callee' (a kEagerFunction)
// was not marked as validated during deserialization.
try {
  f2_main(42, f2_other);
} catch (e) {
  // Expected RuntimeError: illegal cast
}
