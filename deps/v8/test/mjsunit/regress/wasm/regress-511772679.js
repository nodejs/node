// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-validation --no-wasm-native-module-cache
// Flags: --allow-natives-syntax --wasm-deopt --wasm-inlining

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_i_i = makeSig([kWasmI32], [kWasmI32]);
let sig_idx = builder.addType(sig_i_i);

let leaf = builder.addFunction("leaf", sig_idx)
  .addBody([kExprLocalGet, 0]).exportFunc();

let callee = builder.addFunction("callee", makeSig([kWasmI32, kWasmFuncRef], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kGCPrefix, kExprRefCast, sig_idx,
    kExprCallRef, sig_idx
  ]).exportFunc();

builder.addFunction("main", makeSig([kWasmI32, kWasmFuncRef], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprCallFunction, callee.index
  ])
  .exportFunc();

builder.addFunction("other", sig_idx).addBody([kExprLocalGet, 0]).exportFunc();

let wasmBytes = builder.toBuffer();
let wasmModule = new WebAssembly.Module(wasmBytes);
let instance = new WebAssembly.Instance(wasmModule);

let f_main = instance.exports.main;
let f_leaf = instance.exports.leaf;
let f_other = instance.exports.other;

// 1. Execute to collect feedback.
for (let i = 0; i < 10; i++) f_main(42, f_leaf);

// 2. Tier up main.
%WasmTierUpFunction(f_main);

// 3. Flush Liftoff code.
%FlushLiftoffCode();

// 4. Serialize.
let serialized = d8.wasm.serializeModule(wasmModule);

// 5. Deserialize.
let module2 = d8.wasm.deserializeModule(serialized, wasmBytes);
let instance2 = new WebAssembly.Instance(module2);

let f2_main = instance2.exports.main;
let f2_other = instance2.exports.other;

// 6. Trigger deopt. Should not throw or DCHECK.
f2_main(42, f2_other);
