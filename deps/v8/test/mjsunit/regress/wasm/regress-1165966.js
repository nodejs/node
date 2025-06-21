// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

// This test case is simplified slightly from a fuzzer-generated test case. It
// causes spills for one of the inputs to kExprI64x2ExtMulHighI32x4U, which the
// codegen incorrectly assumes will always be a register.
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, true);
// Generate function 1 (out of 1).
builder.addFunction(undefined, kSig_i_v)
  .addLocals(kWasmI64, 1)
  .addBodyWithEnd([
// signature: i_v
// body:
kExprI32Const, 0x00,  // i32.const
kExprI32Const, 0x00,  // i32.const
kExprI32Const, 0x00,  // i32.const
kExprSelectWithType, 0x01, 0x7f,  // select
kExprMemoryGrow, 0x00,  // memory.grow
kExprI32Const, 0xb0, 0xde, 0xc9, 0x03,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0x00,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0xb0, 0xe0, 0xc0, 0x01,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kSimdPrefix, kExprI64x2ExtMulHighI32x4U, 0x01,  // i64x2.extmul_high_i32x4_u
kSimdPrefix, kExprF32x4Le,  // f32x4.le
kSimdPrefix, kExprI32x4ExtractLane, 0x00,  // i32x4.extract_lane
kExprI32DivS,  // i32.div_s
kExprEnd,  // end @41
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
assertThrows(
  () => instance.exports.main(),
  WebAssembly.RuntimeError,
  "divide by zero");
