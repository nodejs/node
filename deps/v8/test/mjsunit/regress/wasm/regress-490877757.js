// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();

let struct = builder.addStruct({
  fields: [makeField(kWasmI32, true)], shared: false
});

let shared_struct = builder.addStruct({
  fields: [makeField(kWasmI32, true)], shared: true
});

builder.addFunction("make", makeSig([kWasmI32], [wasmRefType(struct)]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct])
  .exportFunc();

builder.addFunction("make_shared",
                    makeSig([kWasmI32], [wasmRefType(shared_struct)]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, shared_struct])
  .exportFunc();

let instance = builder.instantiate();

let obj = instance.exports.make(111);

let sharedObj = instance.exports.make_shared(111);

new WeakRef(obj);
assertThrows(() => new WeakRef(sharedObj), TypeError);
