// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --turbo-force-mid-tier-regalloc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false);
builder.addFunction(undefined, kSig_i_iii)
  .addBody([
    ...wasmS128Const(new Array(16).fill(0)),        // s128.const
    kSimdPrefix, kExprF64x2ConvertLowI32x4U, 0x01,  // f64x2.convert_low_i32x4_u
    kSimdPrefix, kExprI64x2UConvertI32x4Low, 0x01,  // i64x2.convert_i32x4_low_u
    kSimdPrefix, kExprI64x2BitMask, 0x01,           // i64x2.bitmask
    ...wasmF64Const(0),                             // f64.const
    kNumericPrefix, kExprI32SConvertSatF64,         // i32.trunc_sat_f64_s
    ...wasmI32Const(0),                             // i32.const
    kExprCallFunction, 0,                           // call
    kExprDrop,                                      // drop
    ...wasmI32Const(0),                             // i32.const
    ...wasmI64Const(0),                             // i64.const
    kExprI64StoreMem16, 0x00, 0x00,                 // i64.store16
    ...wasmF32Const(0),                             // f32.const
    kExprF32Sqrt,                                   // f32.sqrt
    kExprI32UConvertF32,                            // i32.trunc_f32_u
]);
builder.toModule();
