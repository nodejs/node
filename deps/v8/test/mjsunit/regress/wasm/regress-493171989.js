// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let struct_idx = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  is_shared: true,
});

builder.addFunction("create", makeSig([], [wasmRefType(struct_idx)]))
  .addBody([
    kGCPrefix, kExprStructNewDefault, struct_idx
  ])
  .exportFunc();

builder.addFunction("test", makeSig([wasmRefType(struct_idx), kWasmI32], []))
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kAtomicPrefix, kExprStructAtomicSet,
    kAtomicAcqRel,
    struct_idx,
    0x00
  ])
  .exportFunc();

let instance = builder.instantiate();
let struct = instance.exports.create();
instance.exports.test(struct, 0);
