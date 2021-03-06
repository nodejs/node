// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig = builder.addType(makeSig([], [kWasmF64]));
builder.addFunction(undefined, sig)
  .addLocals(kWasmF32, 5).addLocals(kWasmF64, 3)
  .addBody([
kExprBlock, kWasmF64,
  kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f,
  kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  kExprI32Const, 0,
  kExprIf, kWasmI32,
    kExprI32Const, 0,
  kExprElse,
    kExprI32Const, 1,
    kExprEnd,
  kExprBrIf, 0,
  kExprUnreachable,
kExprEnd
]);
builder.instantiate();
