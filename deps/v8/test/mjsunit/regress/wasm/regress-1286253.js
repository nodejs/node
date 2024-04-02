// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --turbo-force-mid-tier-regalloc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_i_iii)
  .addBody([
    ...wasmS128Const(new Array(16).fill(0)),    // s128.const
    kSimdPrefix, kExprI8x16ExtractLaneU, 0x00,  // i8x16.extract_lane_u
    ...wasmS128Const(new Array(16).fill(0)),    // s128.const
    kSimdPrefix, kExprF32x4ExtractLane, 0x00,   // f32x4.extract_lane
    kNumericPrefix, kExprI64SConvertSatF32,     // i64.trunc_sat_f32_s
    kExprF32Const, 0x13, 0x00, 0x00, 0x00,      // f32.const
    kNumericPrefix, kExprI64SConvertSatF32,     // i64.trunc_sat_f32_s
    kExprI64Ior,                                // i64.or
    kExprI32ConvertI64,                         // i32.wrap_i64
    ...wasmF32Const(0),                         // f32.const
    kNumericPrefix, kExprI64SConvertSatF32,     // i64.trunc_sat_f32_s
    kExprI32ConvertI64,                         // i32.wrap_i64
    kExprSelect,                                // select
]);
builder.toModule();
