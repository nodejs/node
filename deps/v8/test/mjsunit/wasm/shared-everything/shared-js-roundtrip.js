// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();

let struct =
    builder.addStruct({fields: [makeField(kWasmI32, true)], shared: false});
let shared_struct =
    builder.addStruct({fields: [makeField(kWasmI32, true)], shared: true});

builder.addFunction("producer", makeSig([], [wasmRefType(struct)]))
  .addBody([kGCPrefix, kExprStructNewDefault, struct])
  .exportFunc();

builder.addFunction(
    "shared_producer", makeSig([], [wasmRefType(shared_struct)]))
  .addBody([kGCPrefix, kExprStructNewDefault, shared_struct])
  .exportFunc();

let struct_type = wasmRefNullType(kWasmStructRef);
builder.addFunction("id", makeSig([struct_type], [struct_type]))
  .addBody([kExprLocalGet, 0])
  .exportFunc();

let shared_struct_type = wasmRefNullType(kWasmStructRef).shared();
builder.addFunction(
    "shared_id", makeSig([shared_struct_type], [shared_struct_type]))
  .addBody([kExprLocalGet, 0])
  .exportFunc();

builder.addFunction("eq_id", makeSig([kWasmEqRef], [kWasmEqRef]))
  .addBody([kExprLocalGet, 0])
  .exportFunc();

let shared_eq_type = wasmRefNullType(kWasmEqRef).shared();

builder.addFunction("shared_eq_id", makeSig([shared_eq_type], [shared_eq_type]))
  .addBody([kExprLocalGet, 0])
  .exportFunc();

builder.addFunction("any_id", makeSig([kWasmAnyRef], [kWasmAnyRef]))
  .addBody([kExprLocalGet, 0])
  .exportFunc();

let shared_any_type = wasmRefNullType(kWasmAnyRef).shared();

builder.addFunction("shared_any_id",
                    makeSig([shared_any_type], [shared_any_type]))
  .addBody([kExprLocalGet, 0])
  .exportFunc();

builder.addFunction("extern_id", makeSig([kWasmExternRef], [kWasmExternRef]))
  .addBody([kExprLocalGet, 0])
  .exportFunc();

let shared_extern_type = wasmRefNullType(kWasmExternRef).shared();

builder.addFunction("shared_extern_id",
                    makeSig([shared_extern_type], [shared_extern_type]))
  .addBody([kExprLocalGet, 0])
  .exportFunc();


let wasm = builder.instantiate().exports;

function error(fn) {
  assertThrows(fn, TypeError,
               'type incompatibility when transforming from/to JS');
}

// We can roundtrip a non-shared struct as structref and a shared struct as
// (shared structref)...
wasm.id(wasm.producer());
wasm.shared_id(wasm.shared_producer());

// ... but not vice versa.
error(() => wasm.id(wasm.shared_producer()));
error(() => wasm.shared_id(wasm.producer()));

// Same for eqref.
wasm.eq_id(wasm.producer());
wasm.shared_eq_id(wasm.shared_producer());
error(() => wasm.eq_id(wasm.shared_producer()));
error(() => wasm.shared_eq_id(wasm.producer()));

// For anyref (nullable) there are no type errors.
wasm.any_id(wasm.producer());
wasm.any_id(wasm.shared_producer());
// For shared anyref, non-shareable objects throw.
wasm.shared_any_id(wasm.shared_producer());
assertEquals("hello", wasm.shared_any_id("hello"));
assertEquals(42, wasm.shared_any_id(42));
assertEquals(42.5, wasm.shared_any_id(42.5));
error(() => wasm.shared_any_id(wasm.producer()));
error(() => wasm.shared_any_id({}));

// For shared externref, non-shareable objects throw.
wasm.extern_id(wasm.producer());
wasm.extern_id(wasm.shared_producer());
wasm.shared_extern_id(wasm.shared_producer());
assertEquals("hello", wasm.shared_extern_id("hello"));
error(() => wasm.shared_extern_id(wasm.producer()));
error(() => wasm.shared_extern_id({}));
