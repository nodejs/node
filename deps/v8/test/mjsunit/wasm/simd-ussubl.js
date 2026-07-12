// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32);

// Function Naming Scheme:
// These tests test three sizes of addition (ways to divide the 128 bit vector
// register):
// 2D: the output is two double-width lanes  (64 bits wide each)
// 4S: the output is four single-width lanes (32 bits wide each)
// 8H: the output is eight half-width lanes  (16 bits wide each)
// Each of these size variants takes two inputs, each with the same lane count
// but half the width as the output.
// For the smaller input(s), H (High) or L (Low) determines which half of the
// register to read from. Each of these 6 variants each come in both
// S (Signed) and U (Unsigned) variants.

// These tests construct a series of wasm instructions which will* result in
// a particular optimization being triggered, so each test is checking that
// one codepath is producing correct results.
// *: The instruction selection tests passing proves the desired optimisation
// occurs.

// 2D Low
builder.addFunction("main_vector_2D_L_U", makeSig([kWasmI32, kWasmI32], [kWasmI64]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI32x4Splat),
  ...SimdInstr(kExprI64x2UConvertI32x4Low),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI32x4Splat),
  ...SimdInstr(kExprI64x2UConvertI32x4Low),
  ...SimdInstr(kExprI64x2Sub),
  ...SimdInstr(kExprI64x2ExtractLane),0,
]).exportFunc();

builder.addFunction("main_vector_2D_L_S", makeSig([kWasmI32, kWasmI32], [kWasmI64]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI32x4Splat),
  ...SimdInstr(kExprI64x2SConvertI32x4Low),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI32x4Splat),
  ...SimdInstr(kExprI64x2SConvertI32x4Low),
  ...SimdInstr(kExprI64x2Sub),
  ...SimdInstr(kExprI64x2ExtractLane),0,
]).exportFunc();
// 2D High
builder.addFunction("main_vector_2D_H_U", makeSig([kWasmI32, kWasmI32], [kWasmI64]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI32x4Splat),
  ...SimdInstr(kExprI64x2UConvertI32x4High),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI32x4Splat),
  ...SimdInstr(kExprI64x2UConvertI32x4High),
  ...SimdInstr(kExprI64x2Sub),
  ...SimdInstr(kExprI64x2ExtractLane),0,
]).exportFunc();

builder.addFunction("main_vector_2D_H_S", makeSig([kWasmI32, kWasmI32], [kWasmI64]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI32x4Splat),
  ...SimdInstr(kExprI64x2SConvertI32x4High),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI32x4Splat),
  ...SimdInstr(kExprI64x2SConvertI32x4High),
  ...SimdInstr(kExprI64x2Sub),
  ...SimdInstr(kExprI64x2ExtractLane),0,
]).exportFunc();


// 4S Low
builder.addFunction("main_vector_4S_L_U", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI16x8Splat),
  ...SimdInstr(kExprI32x4UConvertI16x8Low),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI16x8Splat),
  ...SimdInstr(kExprI32x4UConvertI16x8Low),
  ...SimdInstr(kExprI32x4Sub),
  ...SimdInstr(kExprI32x4ExtractLane),0,
]).exportFunc();

builder.addFunction("main_vector_4S_L_S", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI16x8Splat),
  ...SimdInstr(kExprI32x4SConvertI16x8Low),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI16x8Splat),
  ...SimdInstr(kExprI32x4SConvertI16x8Low),
  ...SimdInstr(kExprI32x4Sub),
  ...SimdInstr(kExprI32x4ExtractLane),0,
]).exportFunc();

// 4S High
builder.addFunction("main_vector_4S_H_U", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI16x8Splat),
  ...SimdInstr(kExprI32x4UConvertI16x8High),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI16x8Splat),
  ...SimdInstr(kExprI32x4UConvertI16x8High),
  ...SimdInstr(kExprI32x4Sub),
  ...SimdInstr(kExprI32x4ExtractLane),0,
]).exportFunc();

builder.addFunction("main_vector_4S_H_S", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI16x8Splat),
  ...SimdInstr(kExprI32x4SConvertI16x8High),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI16x8Splat),
  ...SimdInstr(kExprI32x4SConvertI16x8High),
  ...SimdInstr(kExprI32x4Sub),
  ...SimdInstr(kExprI32x4ExtractLane),0,
]).exportFunc();

// 2H Low
builder.addFunction("main_vector_2H_L_U", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI8x16Splat),
  ...SimdInstr(kExprI16x8UConvertI8x16Low),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI8x16Splat),
  ...SimdInstr(kExprI16x8UConvertI8x16Low),
  ...SimdInstr(kExprI16x8Sub),
  ...SimdInstr(kExprI16x8ExtractLaneU),0,
]).exportFunc();

builder.addFunction("main_vector_2H_L_S", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI8x16Splat),
  ...SimdInstr(kExprI16x8SConvertI8x16Low),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI8x16Splat),
  ...SimdInstr(kExprI16x8SConvertI8x16Low),
  ...SimdInstr(kExprI16x8Sub),
  ...SimdInstr(kExprI16x8ExtractLaneS),0,
]).exportFunc();

// 2H High
builder.addFunction("main_vector_2H_H_U", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI8x16Splat),
  ...SimdInstr(kExprI16x8UConvertI8x16High),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI8x16Splat),
  ...SimdInstr(kExprI16x8UConvertI8x16High),
  ...SimdInstr(kExprI16x8Sub),
  ...SimdInstr(kExprI16x8ExtractLaneU),0,
]).exportFunc();

builder.addFunction("main_vector_2H_H_S", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI8x16Splat),
  ...SimdInstr(kExprI16x8SConvertI8x16High),
  kExprLocalGet, 1,
  ...SimdInstr(kExprI8x16Splat),
  ...SimdInstr(kExprI16x8SConvertI8x16High),
  ...SimdInstr(kExprI16x8Sub),
  ...SimdInstr(kExprI16x8ExtractLaneS),0,
]).exportFunc();

const module = builder.instantiate();

function test_vector_sub(s1, s2, array, signed_func, unsigned_func) {
  for (let i = 0; i < array.length-2; i++) {
    let a = BigInt(array[i]);
    let b = BigInt(array[i+2]);
    let ua = BigInt.asUintN(s1, a);
    let ub = BigInt.asUintN(s1, b);
    let sa = BigInt.asIntN(s1, a);
    let sb = BigInt.asIntN(s1, b);
    assertEquals(
      BigInt.asUintN(s2, ua - ub),
      BigInt.asUintN(s2, BigInt(unsigned_func(
        s1>32 ? ua : Number(ua),
        s1>32 ? ub : Number(ub)
      )))
    );
    assertEquals(
      BigInt.asIntN(s2, sa - sb),
      BigInt.asIntN(s2, BigInt(signed_func(
        s1>32 ? sa : Number(sa),
        s1>32 ? sb : Number(sb)
      )))
    );
  }
}

// 2D
test_vector_sub(32, 64, uint64_array, module.exports.main_vector_2D_L_S,
  module.exports.main_vector_2D_L_U);
test_vector_sub(32, 64, uint64_array, module.exports.main_vector_2D_H_S,
  module.exports.main_vector_2D_H_U);
// 4S
test_vector_sub(16, 32, int32_array, module.exports.main_vector_4S_L_S,
  module.exports.main_vector_4S_L_U);
test_vector_sub(16, 32, int32_array, module.exports.main_vector_4S_H_S,
  module.exports.main_vector_4S_H_U);
// 2H
test_vector_sub(8, 16, int16_array, module.exports.main_vector_2H_L_S,
  module.exports.main_vector_2H_L_U);
test_vector_sub(8, 16, int16_array, module.exports.main_vector_2H_H_S,
  module.exports.main_vector_2H_H_U);
