// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
let builder = new WasmModuleBuilder();
let inner_struct_type = builder.addStruct([makeField(kWasmI32, true)]);
let outer_struct_type =
builder.addStruct([makeField(wasmRefNullType(inner_struct_type), true)]);
builder.addFunction("create_inner",
    makeSig([], [wasmRefType(inner_struct_type)]))
.addBody([
    kGCPrefix, kExprStructNewDefault, inner_struct_type
])
.exportFunc();
builder.addFunction("create_outer",
    makeSig([], [wasmRefType(outer_struct_type)]))
.addBody([
    kGCPrefix, kExprStructNewDefault, outer_struct_type
])
.exportFunc();

builder.addFunction("cmpxchg",
    makeSig([wasmRefType(outer_struct_type), wasmRefNullType(inner_struct_type),
             wasmRefNullType(inner_struct_type)],
            [wasmRefNullType(inner_struct_type)]))
.addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kAtomicPrefix, kExprStructAtomicCompareExchange, 0, outer_struct_type, 0,
])
.exportFunc();

let instance = builder.instantiate();
let create_inner = instance.exports.create_inner;
let create_outer = instance.exports.create_outer;
let cmpxchg = instance.exports.cmpxchg;
let root = create_outer();
for (let i = 0; i < 20; i++) {
    let target = create_inner();
    cmpxchg(root, null, target);
    if (i % 10 === 0) gc();
}
