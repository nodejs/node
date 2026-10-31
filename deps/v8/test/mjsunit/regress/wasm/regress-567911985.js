// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-fp16

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('qfma', makeSig([kWasmF32], [kWasmF32]))
  .addLocals(kWasmS128, 1)
  .addBody([
    kExprLocalGet, 0,
    kSimdPrefix, kExprF16x8Splat,
    kExprLocalTee, 1,
    kExprLocalGet, 1,
    wasmF32Const(1.0),
    kSimdPrefix, kExprF16x8Splat,
    kExprLocalTee, 1,
    kSimdPrefix, kExprF16x8Qfma,
    kSimdPrefix, kExprF16x8ExtractLane, 0,
  ].flat())
  .exportFunc();

builder.addFunction('qfms', makeSig([kWasmF32], [kWasmF32]))
  .addLocals(kWasmS128, 1)
  .addBody([
    kExprLocalGet, 0,
    kSimdPrefix, kExprF16x8Splat,
    kExprLocalTee, 1,
    kExprLocalGet, 1,
    wasmF32Const(10.0),
    kSimdPrefix, kExprF16x8Splat,
    kExprLocalTee, 1,
    kSimdPrefix, kExprF16x8Qfms,
    kSimdPrefix, kExprF16x8ExtractLane, 0,
  ].flat())
  .exportFunc();

builder.addFunction('demote_zero', makeSig([kWasmF32], [kWasmF32]))
  .addBody([
    kExprLocalGet, 0,
    kSimdPrefix, kExprF16x8Splat,
    kSimdPrefix, kExprF32x4PromoteLowF16x8,
    kSimdPrefix, kExprF16x8DemoteF32x4Zero,
    kSimdPrefix, kExprF16x8ExtractLane, 7,
  ].flat())
  .exportFunc();

builder.addFunction('gt', makeSig([kWasmF32, kWasmF32], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kSimdPrefix, kExprF16x8Splat,
    kExprLocalGet, 1,
    kSimdPrefix, kExprF16x8Splat,
    kSimdPrefix, kExprF16x8Gt,
    kSimdPrefix, kExprI16x8ExtractLaneS, 0,
  ].flat())
  .exportFunc();

builder.addFunction('replace_lane', makeSig([kWasmF32], [kWasmF32]))
  .addLocals(kWasmS128, 1)
  .addBody([
    kExprLocalGet, 1,
    kExprLocalGet, 0,
    kSimdPrefix, kExprF16x8ReplaceLane, 0,
    kSimdPrefix, kExprF16x8ExtractLane, 0,
  ].flat())
  .exportFunc();

const instance = builder.instantiate();
assertEquals(5.0, instance.exports.qfma(2.0));
assertEquals(6.0, instance.exports.qfms(2.0));
assertEquals(0.0, instance.exports.demote_zero(1.5));
assertEquals(-1, instance.exports.gt(2.0, 1.0));
assertEquals(0, instance.exports.gt(1.0, 2.0));
assertEquals(3.5, instance.exports.replace_lane(3.5));
