// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --fuzzing --empty-shared-heap --wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();

const node_type_index = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  shared: true
});

const node_ref_type = wasmRefType(node_type_index);

const create_sig = builder.addType(
  makeSig([kWasmI32], [node_ref_type])
);

builder.addFunction("create", create_sig)
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprStructNew, ...wasmUnsignedLeb(node_type_index),
  ])
  .exportFunc();

// Allocate a shared struct
const instance = builder.instantiate();
const create = instance.exports.create;
const node = create(42);

// This GC would fail the empty-shared-heap CHECKs, except that the --fuzzing
// flag means that the --wasm-shared overrides the --empty-shared-heap.
gc();
