// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addMemory(1, 1, false);
builder.exportMemoryAs('memory');

let struct_type_index = builder.addStruct([makeField(kWasmI32, true)]);
let struct_ref_type = wasmRefType(struct_type_index);

builder.addFunction("create_struct", makeSig([kWasmI32], [struct_ref_type]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprStructNew, ...wasmUnsignedLeb(struct_type_index)
  ])
  .exportFunc();

builder.addFunction('test', makeSig([kWasmI32, kWasmI32, kWasmI32, struct_ref_type], [kWasmI32]))
    .addBody([
      // memory.copy(dst=local 0, src=local 1, size=local 2)
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kNumericPrefix, kExprMemoryCopy, 0, 0,

      // Read from struct_ref (local 3) and return
      kExprLocalGet, 3,
      kGCPrefix, kExprStructGet, ...wasmUnsignedLeb(struct_type_index), 0,
    ])
    .exportFunc();

const instance = builder.instantiate();
let struct_obj = instance.exports.create_struct(42);

let res = instance.exports.test(0, 4, 10, struct_obj);
assertEquals(42, res);
