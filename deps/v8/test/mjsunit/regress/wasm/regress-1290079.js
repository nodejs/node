// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --turbo-force-mid-tier-regalloc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, makeSig([], [kWasmS128]))
  .addBody([
...wasmS128Const(new Array(16).fill(0)),  // s128.const
...wasmS128Const(new Array(16).fill(0)),  // s128.const
...wasmS128Const(new Array(16).fill(0)),  // s128.const
kExprI32Const, 0x00,  // i32.const
kSimdPrefix, kExprI8x16ReplaceLane, 0x00,  // i8x16.replace_lane
kSimdPrefix, kExprI8x16GtS,  // i8x16.gt_s
kSimdPrefix, kExprI16x8Ne,  // i16x8.ne
...wasmS128Const(new Array(16).fill(1)),  // s128.const
kExprI32Const, 0x00,  // i32.const
kSimdPrefix, kExprI8x16ReplaceLane, 0x00,  // i8x16.replace_lane
kExprI32Const, 0x00,  // i32.const
kSimdPrefix, kExprI8x16ReplaceLane, 0x00,  // i8x16.replace_lane
...wasmS128Const(new Array(16).fill(2)),  // s128.const
kSimdPrefix, kExprI16x8Eq,  // i16x8.eq
kSimdPrefix, kExprI16x8Ne,  // i16x8.ne
...wasmS128Const(new Array(16).fill(1)),  // s128.const
...wasmS128Const(new Array(16).fill(1)),  // s128.const
...wasmS128Const(new Array(16).fill(0)),  // s128.const
kSimdPrefix, kExprI16x8AddSatU, 0x01,  // i16x8.add_sat_u
...wasmS128Const(new Array(16).fill(0)),  // s128.const
...wasmS128Const(new Array(16).fill(0)),  // s128.const
kSimdPrefix, kExprI16x8Sub, 0x01,  // i16x8.sub
kSimdPrefix, kExprI64x2ExtMulHighI32x4U, 0x01,  // i64x2.extmul_high_i32x4_u
kSimdPrefix, kExprI64x2ExtMulLowI32x4S, 0x01,  // i64x2.extmul_low_i32x4_s
kExprF32Const, 0x00, 0x00, 0x00, 0x00,  // f32.const
kExprF32Const, 0x00, 0x00, 0x00, 0x00,  // f32.const
kExprF32Mul,  // f32.mul
kExprF32Const, 0x00, 0x00, 0x00, 0x00,  // f32.const
...wasmS128Const(new Array(16).fill(0)),  // s128.const
kSimdPrefix, kExprI16x8ExtractLaneS, 0x00,  // i16x8.extract_lane_s
kExprSelect,  // select
kNumericPrefix, kExprI32SConvertSatF32,  // i32.trunc_sat_f32_s
kSimdPrefix, kExprI8x16ReplaceLane, 0x00,  // i8x16.replace_lane
kSimdPrefix, kExprI16x8Ne,  // i16x8.ne
]);
builder.toModule();
