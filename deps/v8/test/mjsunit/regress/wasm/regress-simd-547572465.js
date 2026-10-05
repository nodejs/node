// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-enable-avx --liftoff-only

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Test i16x8.q15mulr_sat_s when destination aliases rhs.
{
  const builder = new WasmModuleBuilder();
  builder.addFunction('test_strict', makeSig([kWasmI32, kWasmI32], [kWasmI32]))
    .addLocals(kWasmS128, 1)
    .addBody([
      kExprLocalGet, 0,
      ...SimdInstr(kExprI16x8Splat),
      kExprLocalSet, 2, // lhs (in local)
      kExprLocalGet, 2, // push lhs
      kExprLocalGet, 1,
      ...SimdInstr(kExprI16x8Splat), // push rhs (temporary)
      ...SimdInstr(kExprI16x8Q15MulRSatS),
      ...SimdInstr(kExprI16x8ExtractLaneS), 0,
    ])
    .exportFunc();

  const instance = builder.instantiate();
  // (16384 * 8192 + 0x4000) >> 15 = 4096
  // If miscompiled (lhs * lhs): (16384 * 16384 + 0x4000) >> 15 = 8192
  assertEquals(4096, instance.exports.test_strict(16384, 8192));
}

// Test i16x8.relaxed_q15mulr_s when destination aliases rhs.
{
  const builder = new WasmModuleBuilder();
  builder.addFunction('test_relaxed', makeSig([kWasmI32, kWasmI32], [kWasmI32]))
    .addLocals(kWasmS128, 1)
    .addBody([
      kExprLocalGet, 0,
      ...SimdInstr(kExprI16x8Splat),
      kExprLocalSet, 2, // lhs (in local)
      kExprLocalGet, 2, // push lhs
      kExprLocalGet, 1,
      ...SimdInstr(kExprI16x8Splat), // push rhs (temporary)
      kSimdPrefix, ...kExprI16x8RelaxedQ15MulRS,
      ...SimdInstr(kExprI16x8ExtractLaneS), 0,
    ])
    .exportFunc();

  const instance = builder.instantiate();
  assertEquals(4096, instance.exports.test_relaxed(16384, 8192));
}
