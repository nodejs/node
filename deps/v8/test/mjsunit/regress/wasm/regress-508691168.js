// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --allow-natives-syntax --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let struct_type = builder.addStruct([makeField(kWasmEqRef, true)]);
builder.addFunction("create", makeSig([], [wasmRefType(struct_type)]))
.addBody([
  kExprRefNull, kEqRefCode,
  kGCPrefix, kExprStructNew, ...wasmUnsignedLeb(struct_type),
]).exportFunc();

builder.addFunction("cas",
  makeSig([wasmRefType(struct_type), kWasmEqRef, kWasmEqRef], [kWasmEqRef]))
.addBody([
  kExprLocalGet, 0,
  kExprLocalGet, 1,
  kExprLocalGet, 2,
  kAtomicPrefix, kExprStructAtomicCompareExchange, 0, struct_type, 0,
]).exportFunc();

builder.addFunction("get", makeSig([wasmRefType(struct_type)], [kWasmEqRef]))
.addBody([
  kExprLocalGet, 0,
]);

let instance = builder.instantiate();
let wasm = instance.exports;
let s = wasm.create();
gc();
gc();
let target = wasm.create();
wasm.cas(s, null, target);
