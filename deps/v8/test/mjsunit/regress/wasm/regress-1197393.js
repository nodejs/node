// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addType(makeSig([kWasmI32, kWasmI64, kWasmF64, kWasmI64], []));
builder.addType(makeSig([kWasmF64], [kWasmF64]));
// Generate function 1 (out of 2).
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
// signature: v_ildl
// body:
kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f,  // f64.const
kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f,  // f64.const
kExprLocalGet, 0x00,  // local.get
kExprI32Const, 0x82, 0x7f,  // i32.const
kExprI32DivS,  // i32.div_s
kExprSelect,  // select
kExprCallFunction, 0x01,  // call function #1: d_d
kExprDrop,  // drop
kExprEnd,  // end @29
]);
// Generate function 2 (out of 2).
builder.addFunction(undefined, 1 /* sig */)
  .addBodyWithEnd([
// signature: d_d
// body:
kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f,  // f64.const
kExprEnd,  // end @10
]);
const instance = builder.instantiate();
