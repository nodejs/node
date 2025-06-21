// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addType(makeSig([kWasmF32, kWasmF32, kWasmI32, kWasmI32, kWasmI32, kWasmExternRef, kWasmI32, kWasmI32, kWasmI32, kWasmI32], [kWasmI64]));
// Generate function 1 (out of 2).
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
// signature: l_ffiiiniiii
// body:
]);
// Generate function 2 (out of 2).
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
// signature: l_ffiiiniiii
// body:
kExprLocalGet, 0x00,  // local.get
kExprLocalGet, 0x01,  // local.get
kExprLocalGet, 0x02,  // local.get
kExprLocalGet, 0x03,  // local.get
kExprI32Const, 0x05,  // i32.const
kExprLocalGet, 0x05,  // local.get
kExprLocalGet, 0x06,  // local.get
kExprLocalGet, 0x07,  // local.get
kExprI32Const, 0x5b,  // i32.const
kExprI32Const, 0x30,  // i32.const
kExprCallFunction, 0x01,  // call function #1: l_ffiiiniiii
kExprLocalGet, 0x00,  // local.get
kExprLocalGet, 0x01,  // local.get
kExprLocalGet, 0x02,  // local.get
kExprLocalGet, 0x03,  // local.get
kExprLocalGet, 0x07,  // local.get
kExprLocalGet, 0x05,  // local.get
kExprLocalGet, 0x06,  // local.get
kExprLocalGet, 0x07,  // local.get
kExprI32Const, 0x7f,  // i32.const
kExprI64DivS,  // i64.div_s
kExprF64Eq,  // f64.eq
kExprI32DivU,  // i32.div_u
kExprTableGet, 0x7f,  // table.get
kExprI64ShrS,  // i64.shr_s
]);
assertThrows(function() { builder.instantiate(); }, WebAssembly.CompileError);
