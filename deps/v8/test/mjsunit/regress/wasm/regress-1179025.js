// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1, false, true);
builder.addType(makeSig([], []));
builder.addType(makeSig([kWasmI64], [kWasmF32]));
// Generate function 1 (out of 2).
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
// signature: v_v
// body:
kExprNop,  // nop
kExprEnd,  // end @2
]);
// Generate function 2 (out of 2).
builder.addFunction(undefined, 1 /* sig */)
  .addLocals(kWasmI64, 1)
  .addBodyWithEnd([
// signature: f_l
// body:
kExprBlock, kWasmF32,  // block @3 f32
  kExprI32Const, 0x00,  // i32.const
  kExprI32Const, 0x01,  // i32.const
  kExprIf, kWasmI64,  // if @9 i64
    kExprI64Const, 0x00,  // i64.const
  kExprElse,  // else @13
    kExprUnreachable,  // unreachable
    kExprEnd,  // end @15
  kAtomicPrefix, kExprI64AtomicStore, 0x03, 0x04,  // i64.atomic.store64
  kExprF32Const, 0x00, 0x00, 0x00, 0x00,  // f32.const
  kExprEnd,  // end @25
kExprDrop,  // drop
kExprF32Const, 0x00, 0x00, 0x80, 0x51,  // f32.const
kExprEnd,  // end @32
]);
builder.instantiate();
