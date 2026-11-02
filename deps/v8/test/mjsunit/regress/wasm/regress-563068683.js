// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared --verify-heap

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Large enough to allocate the array in Large Object Space.
const ARRAY_SIZE = 200_000;

let builder = new WasmModuleBuilder();
let $array = builder.addArray(kWasmI8, {shared: true});
builder.addFunction('create_array', makeSig([], [wasmRefType($array)]))
  .exportFunc()
  .addBody([
    ...wasmI32Const(ARRAY_SIZE),
    kGCPrefix, kExprArrayNewDefault, $array
  ]);

let instance = builder.instantiate();
let array = instance.exports.create_array();
