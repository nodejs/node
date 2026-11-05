// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

// Test narrowing right-shifted SIMD sums and differences with saturating
// conversions.

const configs = [
  {
    laneBits: 16,
    values: int16_array,
    type: kWasmI32,
    splat: kExprI16x8Splat,
    extract: kExprI8x16ExtractLaneS,
    add: kExprI16x8Add,
    sub: kExprI16x8Sub,
    shrS: kExprI16x8ShrS,
    shrU: kExprI16x8ShrU,
    narrowS: kExprI8x16SConvertI16x8,
    narrowU: kExprI8x16UConvertI16x8
  },
  {
    laneBits: 32,
    values: int32_array,
    type: kWasmI32,
    splat: kExprI32x4Splat,
    extract: kExprI16x8ExtractLaneS,
    add: kExprI32x4Add,
    sub: kExprI32x4Sub,
    shrS: kExprI32x4ShrS,
    shrU: kExprI32x4ShrU,
    narrowS: kExprI16x8SConvertI32x4,
    narrowU: kExprI16x8UConvertI32x4
  },
];

function addConvertCase(config, testAdd, arithmeticShift, lane) {
  const builder = new WasmModuleBuilder();
  const {type, splat, extract, laneBits} = config;
  const binop = testAdd ? config.add : config.sub;
  const shift = arithmeticShift ? config.shrS : config.shrU;

  builder.addFunction(
      'convert', makeSig([type, type, type, type], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        ...SimdInstr(splat),
        kExprLocalGet, 1,
        ...SimdInstr(splat),
        ...SimdInstr(binop),
        ...wasmI32Const(laneBits / 2),
        ...SimdInstr(shift),
        kExprLocalGet, 2,
        ...SimdInstr(splat),
        kExprLocalGet, 3,
        ...SimdInstr(splat),
        ...SimdInstr(binop),
        ...wasmI32Const(laneBits / 2),
        ...SimdInstr(shift),
        ...SimdInstr(arithmeticShift ? config.narrowS : config.narrowU),
        ...SimdInstr(extract), lane,
      ])
      .exportFunc();

  const wasm = builder.instantiate().exports;
  testResults(config, testAdd, wasm.convert);
}

function testResults(config, testAdd, func) {
  const narrowBits = config.laneBits / 2;
  for (const left of config.values) {
    for (const right of config.values) {
      const a = BigInt(left);
      const b = BigInt(right);
      const value = BigInt.asUintN(config.laneBits, testAdd ? a + b : a - b);
      const shifted = value >> BigInt(narrowBits);
      const expected = Number(BigInt.asIntN(narrowBits, shifted));
      assertEquals(expected, func(left, right, left, right));
    }
  }
}

for (const config of configs) {
  for (const testAdd of [false, true]) {
    for (const arithmeticShift of [false, true]) {
      addConvertCase(config, testAdd, arithmeticShift, 0);
      addConvertCase(config, testAdd, arithmeticShift, 128 / config.laneBits);
    }
  }
}
