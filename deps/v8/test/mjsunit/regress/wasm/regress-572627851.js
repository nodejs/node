// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared --no-liftoff

// Tests that we mark a value as aliasing when we set it as a struct field.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let inner = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  is_shared: true,
});
let outer = builder.addStruct({
  fields: [makeField(wasmRefNullType(inner), true)],
  is_shared: true,
});

builder.addFunction("test_struct_atomic_set", makeSig([], [kWasmI32]))
  .addLocals(wasmRefType(inner), 1)
  .addLocals(wasmRefType(outer), 1)
  .addBody([
    kGCPrefix, kExprStructNewDefault, inner,
    kExprLocalSet, 0,
    kGCPrefix, kExprStructNewDefault, outer,
    kExprLocalSet, 1,
    kExprLocalGet, 1,
    kExprLocalGet, 0,
    // outer.f = s
    kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, outer, 0,
    kExprLocalGet, 0,
    kExprI32Const, 11,
    kGCPrefix, kExprStructSet, inner, 0,  // s.f = 11
    kExprLocalGet, 1,
    kGCPrefix, kExprStructGet, outer, 0,
    kExprI32Const, 22,
    kGCPrefix, kExprStructSet, inner, 0,  // outer.f.f = 22
    kExprLocalGet, 0,
    kGCPrefix, kExprStructGet, inner, 0,  // s.f -> must be 22
  ])
  .exportFunc();

let instance = builder.instantiate();
assertEquals(22, instance.exports.test_struct_atomic_set());
