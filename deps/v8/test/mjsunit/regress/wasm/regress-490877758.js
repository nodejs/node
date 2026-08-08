// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let structShared = builder.addStruct({
  fields: [makeField(kWasmI32, true)], shared: true});
let table = builder.addTable(wasmRefNullType(structShared), 10, 10);
let passive = builder.addPassiveElementSegment(
  [[kGCPrefix, kExprStructNewDefault, structShared]],
  wasmRefNullType(structShared)
);

builder.addFunction("init", kSig_v_v)
  .addBody([
    kExprI32Const, 0,   // dst
    kExprI32Const, 0,   // src
    kExprI32Const, 1,   // count
    kNumericPrefix, kExprTableInit, passive, table.index,
  ])
  .exportFunc();

instance = builder.instantiate();
instance.exports.init();
