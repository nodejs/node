// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-fp16 --no-enable-avx2 --no-enable-fma3

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
builder.addFunction('test_store', kSig_v_f)
  .addBody([
    wasmI32Const(0),
    kExprLocalGet, 0,
    kNumericPrefix, kExprF32StoreF16, 0, 0,
  ].flat())
  .exportFunc();
builder.addFunction('test_load', kSig_f_v)
  .addBody([
    wasmI32Const(0),
    kNumericPrefix, kExprF32LoadF16, 0, 0,
  ].flat())
  .exportFunc();
builder.addFunction('test_simd', kSig_f_f)
  .addLocals(kWasmS128, 2)
  .addBody([
    // v0 = min(splat(x), splat(2.0)) = 1.5 (or NaN if x is NaN)
    kExprLocalGet, 0,
    kSimdPrefix, kExprF16x8Splat,
    wasmF32Const(2.0),
    kSimdPrefix, kExprF16x8Splat,
    kSimdPrefix, kExprF16x8Min,
    // v1 = max(v0, splat(1.0)) = 1.5 (or NaN if x is NaN)
    wasmF32Const(1.0),
    kSimdPrefix, kExprF16x8Splat,
    kSimdPrefix, kExprF16x8Max,
    kExprLocalTee, 2,
    // v2 = qfma(v1, 2.0, 1.0) = 1.5 * 2.0 + 1.0 = 4.0
    wasmF32Const(2.0),
    kSimdPrefix, kExprF16x8Splat,
    wasmF32Const(1.0),
    kSimdPrefix, kExprF16x8Splat,
    kSimdPrefix, kExprF16x8Qfma,
    // v3 = qfms(v2, 0.5, 5.0) = 5.0 - 4.0 * 0.5 = 3.0
    wasmF32Const(0.5),
    kSimdPrefix, kExprF16x8Splat,
    wasmF32Const(5.0),
    kSimdPrefix, kExprF16x8Splat,
    kSimdPrefix, kExprF16x8Qfms,
    // Round-trip through signed and unsigned i16x8 conversions (still 3.0).
    kSimdPrefix, kExprI16x8SConvertF16x8,
    kSimdPrefix, kExprF16x8SConvertI16x8,
    kSimdPrefix, kExprI16x8UConvertF16x8,
    kSimdPrefix, kExprF16x8UConvertI16x8,
    kExprLocalTee, 1,
    // Compare: (1.5 < 3.0) & (3.0 == 3.0) & (1.5 != 3.0) & (1.5 <= 3.0) = -1
    kExprLocalGet, 0,
    kSimdPrefix, kExprF16x8Splat,
    kExprLocalGet, 1,
    kSimdPrefix, kExprF16x8Lt,
    kExprLocalGet, 1,
    kExprLocalGet, 1,
    kSimdPrefix, kExprF16x8Eq,
    kSimdPrefix, kExprS128And,
    kExprLocalGet, 0,
    kSimdPrefix, kExprF16x8Splat,
    kExprLocalGet, 1,
    kSimdPrefix, kExprF16x8Ne,
    kSimdPrefix, kExprS128And,
    kExprLocalGet, 0,
    kSimdPrefix, kExprF16x8Splat,
    kExprLocalGet, 1,
    kSimdPrefix, kExprF16x8Le,
    kSimdPrefix, kExprS128And,
    // Convert -1 (i16x8) to -1.0 (f16x8) and subtract from 3.0 -> 4.0,
    // then add 0.5 -> 4.5, and max with v1 (1.5 or NaN).
    kSimdPrefix, kExprF16x8SConvertI16x8,
    kSimdPrefix, kExprF16x8Sub,
    wasmF32Const(0.5),
    kSimdPrefix, kExprF16x8Splat,
    kSimdPrefix, kExprF16x8Add,
    kExprLocalGet, 2,
    kSimdPrefix, kExprF16x8Max,
    kExprLocalTee, 1,
    // Check both low (lane 0) and high (lane 7) lanes.
    kSimdPrefix, kExprF16x8ExtractLane, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprF16x8ExtractLane, 7,
    kExprF32Add,
  ].flat())
  .exportFunc();

let instance = builder.instantiate();
instance.exports.test_store(1.5);
assertEquals(1.5, instance.exports.test_load());
assertEquals(9.0, instance.exports.test_simd(1.5));
assertTrue(isNaN(instance.exports.test_simd(NaN)));
