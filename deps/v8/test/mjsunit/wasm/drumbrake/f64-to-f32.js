// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --drumbrake-register-optimization
// Flags: --no-drumbrake-super-instructions

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestF64ToF32() {
  const maxFloat = (2 - 2 ** -23) * 2 ** 127;
  const overflowMidpoint = 2 ** 128 - 2 ** 103;
  const doubleUlp = 2 ** 75;
  const positiveCases = [
    [0, 0x00000000],
    [Number.MIN_VALUE, 0x00000000],
    [2 ** -150 - 2 ** -203, 0x00000000],
    [2 ** -150, 0x00000000],
    [2 ** -150 + 2 ** -202, 0x00000001],
    [2 ** -149, 0x00000001],
    [3 * 2 ** -150, 0x00000002],
    [2 ** -126 - 2 ** -149, 0x007fffff],
    [2 ** -126 - 2 ** -150, 0x00800000],
    [2 ** -126, 0x00800000],
    [1, 0x3f800000],
    [1 + 2 ** -24, 0x3f800000],
    [1 + 3 * 2 ** -24, 0x3f800002],
    [1.5, 0x3fc00000],
    [maxFloat - 2 ** 104, 0x7f7ffffe],
    [maxFloat - doubleUlp, 0x7f7fffff],
    [maxFloat, 0x7f7fffff],
    [maxFloat + doubleUlp, 0x7f7fffff],
    [overflowMidpoint - 2 * doubleUlp, 0x7f7fffff],
    [overflowMidpoint - doubleUlp, 0x7f7fffff],
    [overflowMidpoint, 0x7f800000],
    [overflowMidpoint + doubleUlp, 0x7f800000],
    [2 ** 128, 0x7f800000],
    [1e300, 0x7f800000],
    [Number.MAX_VALUE, 0x7f800000],
    [Infinity, 0x7f800000],
  ];
  const cases = [...positiveCases];
  for (const [value, bits] of positiveCases) {
    cases.push([-value, (bits | 0x80000000) >>> 0]);
  }
  cases.push([NaN, NaN]);

  const builder = new WasmModuleBuilder();
  const modes = [
    ['r2r', true, true],
    ['r2s', true, false],
    ['s2r', false, true],
    ['s2s', false, false],
  ];
  for (const [name, fromRegister, toRegister] of modes) {
    // Double negation puts the unchanged input in a register. Nops force
    // stack operands/results instead of passing them in registers.
    builder.addFunction(name, makeSig([kWasmF64], [kWasmI32]))
        .addBody([
          kExprLocalGet, 0,
          ...(fromRegister ? [kExprF64Neg, kExprF64Neg] : [kExprNop]),
          kExprF32ConvertF64,
          ...(toRegister ? [] : [kExprNop]),
          kExprI32ReinterpretF32,
        ])
        .exportFunc();
  }
  builder.addFunction(
      'simd', makeSig([kWasmF64, kWasmF64],
                      [kWasmI32, kWasmI32, kWasmI32, kWasmI32]))
      .addLocals(kWasmS128, 1)
      .addBody([
        kExprLocalGet, 0,
        kSimdPrefix, kExprF64x2Splat,
        kExprLocalGet, 1,
        kSimdPrefix, kExprF64x2ReplaceLane, 1,
        kSimdPrefix, kExprF32x4DemoteF64x2Zero,
        kExprLocalSet, 2,
        kExprLocalGet, 2,
        kSimdPrefix, kExprI32x4ExtractLane, 0,
        kExprLocalGet, 2,
        kSimdPrefix, kExprI32x4ExtractLane, 1,
        kExprLocalGet, 2,
        kSimdPrefix, kExprI32x4ExtractLane, 2,
        kExprLocalGet, 2,
        kSimdPrefix, kExprI32x4ExtractLane, 3,
      ])
      .exportFunc();

  function assertBits(expected, actual) {
    if (Number.isNaN(expected)) {
      assertTrue((actual & 0x7fffffff) > 0x7f800000);
    } else {
      assertEquals(expected, actual >>> 0);
    }
  }

  const instance = builder.instantiate();
  for (let i = 0; i < cases.length; ++i) {
    const [value, expected] = cases[i];
    for (const [name] of modes) {
      assertBits(expected, instance.exports[name](value));
    }
    const [otherValue, otherExpected] = cases[(i + 1) % cases.length];
    const lanes = instance.exports.simd(value, otherValue);
    assertBits(expected, lanes[0]);
    assertBits(otherExpected, lanes[1]);
    assertEquals(0, lanes[2]);
    assertEquals(0, lanes[3]);
  }
})();
