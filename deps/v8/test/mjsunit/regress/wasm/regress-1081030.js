// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-simd

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
// Generate function 1 (out of 1).
builder.addFunction(undefined, 0 /* sig */).addBodyWithEnd([
  // signature: i_iii
  // body:
  kExprF32Const, 0xf8, 0xf8, 0xf8, 0xf8,
  kSimdPrefix, kExprF32x4Splat,         // f32x4.splat
  kExprF32Const, 0xf8, 0xf8, 0xf8, 0xf8,
  kSimdPrefix, kExprF32x4Splat,         // f32x4.splat
  kSimdPrefix, kExprF32x4Min, 0x01,     // f32x4.min
  kSimdPrefix, kExprV32x4AnyTrue, 0x01,  // i32x4.any_true
  kExprEnd,                             // end @16
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
assertEquals(1, instance.exports.main(1, 2, 3));
