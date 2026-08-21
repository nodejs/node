// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function test() {
  let builder = new WasmModuleBuilder();
  let struct_type = builder.addStruct({
    fields: [makeField(kWasmI32, true)],
    shared: true
  });
  builder.addFunction("create", makeSig([], [wasmRefType(struct_type)]))
    .addBody([kExprI32Const, 0, kGCPrefix, kExprStructNew, struct_type])
    .exportFunc();
  let instance = builder.instantiate();
  let shared_obj = instance.exports.create();

  assertThrows(() => Atomics.notify(shared_obj, 0, 1), TypeError);
}

test();
