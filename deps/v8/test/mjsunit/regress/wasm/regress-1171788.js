// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addType(makeSig(
    [
      kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmFuncRef, kWasmI32, kWasmI32,
      kWasmI32, kWasmI32, kWasmI32
    ],
    [kWasmF64]));
// Generate function 1 (out of 1).
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
// signature: d_iiiiniiiii
// body:
kExprLocalGet, 0x03,  // local.get
kExprLocalGet, 0x08,  // local.get
kExprLocalGet, 0x00,  // local.get
kExprI32Const, 0x01,  // i32.const
kExprLocalGet, 0x04,  // local.get
kExprLocalGet, 0x05,  // local.get
kExprLocalGet, 0x06,  // local.get
kExprLocalGet, 0x00,  // local.get
kExprLocalGet, 0x07,  // local.get
kExprLocalGet, 0x06,  // local.get
kExprCallFunction, 0x00,  // call function #0: d_iiiiniiiii
kExprLocalGet, 0x00,  // local.get
kExprLocalGet, 0x01,  // local.get
kExprLocalGet, 0x00,  // local.get
kExprLocalGet, 0x08,  // local.get
kExprLocalGet, 0x01,  // local.get
kExprLocalGet, 0x00,  // local.get
kExprLocalGet, 0x01,  // local.get
kExprLocalGet, 0x07,  // local.get
kExprLocalGet, 0x08,  // local.get
kExprLocalGet, 0x09,  // local.get
kExprCallFunction, 0x00,  // call function #0: d_iiiiniiiii
kExprUnreachable,  // unreachable
kExprEnd,  // end @46
]);
assertThrows(function() { builder.instantiate(); }, WebAssembly.CompileError);
