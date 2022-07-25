// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --turbo-force-mid-tier-regalloc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
builder.addFunction(undefined, 0 /* sig */)
  .addLocals(kWasmS128, 2)
  .addBody([
    ...wasmF32Const(0),                       // f32.const
    ...wasmI32Const(0),                       // f32.const
    kExprF32SConvertI32,                      // f32.convert_i32_s
    kExprLocalGet, 3,                         // local.get
    kSimdPrefix, kExprI64x2AllTrue, 0x01,     // i64x2.all_true
    kExprSelect,                              // select
    kExprLocalGet, 4,                         // local.get
    ...wasmS128Const(new Array(16).fill(0)),  // s128.const
    kSimdPrefix, kExprI8x16Eq,                // i8x16.eq
    kSimdPrefix, kExprI64x2AllTrue, 0x01,     // i64x2.all_true
    kExprF32SConvertI32,                      // f32.convert_i32_s
    ...wasmS128Const(new Array(16).fill(0)),  // s128.const
    kSimdPrefix, kExprI64x2AllTrue, 0x01,     // i64x2.all_true
    kExprSelect,                              // select
    kExprF32Const, 0x00, 0x00, 0x80, 0x3f,    // f32.const
    kExprF32Ge,                               // f32.ge
]);
builder.toModule();
