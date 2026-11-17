// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

// Test widening multiplication of signed and unsigned 32-bit integers.
const mixed_sign_mul_configs = [
  {
    name: 'i32x4 * u32x4 -> i64x2 low',
    left_values: int32_array,
    right_values: new Uint32Array(int32_array),
    left_ext: kExprI64x2SConvertI32x4Low,
    right_ext: kExprI64x2UConvertI32x4Low,
  },
  {
    name: 'u32x4 * i32x4 -> i64x2 low',
    left_values: new Uint32Array(int32_array),
    right_values: int32_array,
    left_ext: kExprI64x2UConvertI32x4Low,
    right_ext: kExprI64x2SConvertI32x4Low,
  },
  {
    name: 'i32x4 * u32x4 -> i64x2 high',
    left_values: int32_array,
    right_values: new Uint32Array(int32_array),
    left_ext: kExprI64x2SConvertI32x4High,
    right_ext: kExprI64x2UConvertI32x4High,
  },
  {
    name: 'u32x4 * i32x4 -> i64x2 high',
    left_values: new Uint32Array(int32_array),
    right_values: int32_array,
    left_ext: kExprI64x2UConvertI32x4High,
    right_ext: kExprI64x2SConvertI32x4High,
  },
];

function TestMixedSignMul(config) {
  const builder = new WasmModuleBuilder();
  const {left_ext, right_ext} = config;
  builder.addFunction(
      'mixed_sign_mul', makeSig([kWasmI32, kWasmI32], [kWasmI64, kWasmI64]))
      .addLocals(kWasmS128, 1)
      .addBody([
        kExprLocalGet, 0,
        ...SimdInstr(kExprI32x4Splat),
        ...SimdInstr(left_ext),
        kExprLocalGet, 1,
        ...SimdInstr(kExprI32x4Splat),
        ...SimdInstr(right_ext),
        ...SimdInstr(kExprI64x2Mul),
        kExprLocalTee, 2,
        ...SimdInstr(kExprI64x2ExtractLane), 0,
        kExprLocalGet, 2,
        ...SimdInstr(kExprI64x2ExtractLane), 1,
      ])
      .exportFunc();

  const wasm = builder.instantiate().exports;
  for (const left of config.left_values) {
    for (const right of config.right_values) {
      const expected = BigInt(left) * BigInt(right);
      assertEquals(
          [expected, expected], wasm.mixed_sign_mul(left, right),
          `${config.name}, left ${left}, right ${right}`);
    }
  }
}

for (const config of mixed_sign_mul_configs) {
  TestMixedSignMul(config);
}
