// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --turbo-force-mid-tier-regalloc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false, true);
builder.addFunction('main', makeSig([], [kWasmS128]))
  .addBody([
kExprI32Const, 0,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 2,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 3,  // i32.const
kSimdPrefix, kExprI16x8ShrS, 0x01,  // i16x8.shr_s
kExprI32Const, 0xc4, 0x88, 0x91, 0xa2, 0x04,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kSimdPrefix, kExprI16x8ExtAddPairwiseI8x16S,  // i16x8.extadd_pairwise_i8x6_s
kSimdPrefix, kExprI16x8AddSatU, 0x01,  // i16x8.add_sat_u
kExprI32Const, 0xac, 0x92, 0x01,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprF32Const, 0x2b, 0x2b, 0x2b, 0x49,  // f32.const
kSimdPrefix, kExprF32x4ReplaceLane, 0x00,  // f32x4.replace_lane
kSimdPrefix, kExprI16x8ExtAddPairwiseI8x16S,  // i16x8.extadd_pairwise_i8x6_s
kSimdPrefix, kExprI16x8RoundingAverageU, 0x01,  // i16x8.avgr_u
kExprI32Const, 0,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kSimdPrefix, kExprI64x2UConvertI32x4High, 0x01,  // i64x2.convert_i32x4_high_u
kSimdPrefix, kExprI64x2SConvertI32x4High, 0x01,  // i64x2.convert_i32x4_high_s
kExprI32Const, 0,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprF32Const, 0, 0, 0, 0,  // f32.const
kSimdPrefix, kExprF32x4ReplaceLane, 0x00,  // f32x4.replace_lane
kExprI32Const, 0,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kSimdPrefix, kExprI16x8ExtMulLowI8x16U, 0x01,  // i16x8.extmul_low_i8x16_u
kSimdPrefix, kExprI16x8LeU,  // i16x8.le_u
kSimdPrefix, kExprI8x16GtS,  // i8x16.gt_s
kSimdPrefix, kExprI32x4Ne,  // i32x4.ne
]);
builder.toModule();
