// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

// This test is manually reduced from a fuzzer test case at
// https://crbug.com/1161954. This exercises a bug in IA32 instruction
// selection for v128.select, in the AVX case it was too flexible and allowed
// the input operands to be slots, but the code-gen required them to be
// registers.
load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
// Generate function 1 (out of 1).
builder.addFunction(undefined, kSig_i_v)
  .addBodyWithEnd([
// signature: i_v
// body:
kExprI32Const, 0x37,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0xb9, 0xf2, 0xd8, 0x01,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kSimdPrefix, kExprS128AndNot,  // s128.andnot
kExprI32Const, 0xb2, 0xf2, 0x00,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0x00,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0xf2, 0x82, 0x02,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kSimdPrefix, kExprF64x2Max, 0x01,  // f64x2.max
kSimdPrefix, kExprI16x8Add, 0x01,  // i16x8.add
kSimdPrefix, kExprS128Or,  // s128.or
kSimdPrefix, kExprI8x16Neg,  // i8x16.neg
kExprI32Const, 0x8e, 0x1c,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0x00,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0x9d, 0x26,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0xf0, 0xe0, 0x01,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kSimdPrefix, kExprI8x16LeS,  // i8x16.le_s
kSimdPrefix, kExprI8x16LeS,  // i8x16.le_s
kExprI32Const, 0xff, 0xfb, 0x03,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0x93, 0x26,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0x9d, 0x26,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kSimdPrefix, kExprI8x16GtU,  // i8x16.gt_u
kExprI32Const, 0xf0, 0x00,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kSimdPrefix, kExprI16x8Mul, 0x01,  // i16x8.mul
kSimdPrefix, kExprF32x4Ge,  // f32x4.ge
kSimdPrefix, kExprI8x16LeS,  // i8x16.le_s
kSimdPrefix, kExprI8x16LeS,  // i8x16.le_s
kExprI32Const, 0x00,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0xc1, 0x8e, 0x35,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0x0d,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kSimdPrefix, kExprI32x4Ne,  // i32x4.ne
kSimdPrefix, kExprF32x4Ge,  // f32x4.ge
kSimdPrefix, kExprI8x16LeS,  // i8x16.le_s
kExprI32Const, 0xc1, 0x8e, 0x35,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0x0d,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kSimdPrefix, kExprS128Select,  // s128.select
kSimdPrefix, kExprF64x2Div, 0x01,  // f64x2.div
kSimdPrefix, kExprF64x2ExtractLane, 0x00,  // f64x2.extract_lane
kNumericPrefix, kExprI32SConvertSatF64,  // i32.trunc_sat_f64_s
kExprEnd,  // end @142
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
print(instance.exports.main());
