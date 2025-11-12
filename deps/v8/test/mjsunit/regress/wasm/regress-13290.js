// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

builder.addMemory(16, 17);

let main_func = builder.addFunction('main', kSig_i_v).exportFunc().addBody([
  ...wasmI32Const(1),
  ...wasmI32Const(-1),
  kExprI32StoreMem16, 1, 0,
  ...wasmI32Const(0),
  kExprI64LoadMem32U, 2, 0,
  ...wasmI64Const(-32),
  kExprI64ShrU,
  kExprI32ConvertI64,
]);

let instance = builder.instantiate();
let main = instance.exports.main;

for (let i = 0; i < 20; i++) assertEquals(0, main());
%WasmTierUpFunction(main);
assertEquals(0, main());
