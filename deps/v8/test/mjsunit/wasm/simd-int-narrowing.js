// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

// Test signed and unsigned SIMD narrowing.
const builder = new WasmModuleBuilder();

builder.addFunction('i8x16_narrow_s', kSig_ii_ii)
    .addLocals(kWasmS128, 1)
    .addBody([
      kExprLocalGet, 0,
      ...SimdInstr(kExprI16x8Splat),
      kExprLocalGet, 1,
      ...SimdInstr(kExprI16x8Splat),
      ...SimdInstr(kExprI8x16SConvertI16x8),
      kExprLocalTee, 2,
      ...SimdInstr(kExprI8x16ExtractLaneS), 0,
      kExprLocalGet, 2,
      ...SimdInstr(kExprI8x16ExtractLaneS), 8,
    ])
    .exportFunc();

builder.addFunction('i8x16_narrow_u', kSig_ii_ii)
    .addLocals(kWasmS128, 1)
    .addBody([
      kExprLocalGet, 0,
      ...SimdInstr(kExprI16x8Splat),
      kExprLocalGet, 1,
      ...SimdInstr(kExprI16x8Splat),
      ...SimdInstr(kExprI8x16UConvertI16x8),
      kExprLocalTee, 2,
      ...SimdInstr(kExprI8x16ExtractLaneU), 0,
      kExprLocalGet, 2,
      ...SimdInstr(kExprI8x16ExtractLaneU), 8,
    ])
    .exportFunc();

builder.addFunction('i16x8_narrow_s', kSig_ii_ii)
    .addLocals(kWasmS128, 1)
    .addBody([
      kExprLocalGet, 0,
      ...SimdInstr(kExprI32x4Splat),
      kExprLocalGet, 1,
      ...SimdInstr(kExprI32x4Splat),
      ...SimdInstr(kExprI16x8SConvertI32x4),
      kExprLocalTee, 2,
      ...SimdInstr(kExprI16x8ExtractLaneS), 0,
      kExprLocalGet, 2,
      ...SimdInstr(kExprI16x8ExtractLaneS), 4,
    ])
    .exportFunc();

builder.addFunction('i16x8_narrow_u', kSig_ii_ii)
    .addLocals(kWasmS128, 1)
    .addBody([
      kExprLocalGet, 0,
      ...SimdInstr(kExprI32x4Splat),
      kExprLocalGet, 1,
      ...SimdInstr(kExprI32x4Splat),
      ...SimdInstr(kExprI16x8UConvertI32x4),
      kExprLocalTee, 2,
      ...SimdInstr(kExprI16x8ExtractLaneU), 0,
      kExprLocalGet, 2,
      ...SimdInstr(kExprI16x8ExtractLaneU), 4,
    ])
    .exportFunc();

const wasm = builder.instantiate().exports;

function testVectorNarrow(values, min, max, func) {
  // Clamp signed source values to the destination range [min, max].
  for (const left of values) {
    for (const right of values) {
      assertEquals(
          [Math.max(min, Math.min(max, left)),
           Math.max(min, Math.min(max, right))],
          func(left, right), `left ${left}, right ${right}`);
    }
  }
}

// i16x8 -> i8x16.
testVectorNarrow(int16_array, -128, 127, wasm.i8x16_narrow_s);
testVectorNarrow(int16_array, 0, 255, wasm.i8x16_narrow_u);

// i32x4 -> i16x8.
testVectorNarrow(int32_array, -32768, 32767, wasm.i16x8_narrow_s);
testVectorNarrow(int32_array, 0, 65535, wasm.i16x8_narrow_u);
