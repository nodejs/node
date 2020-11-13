// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-simd

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false);
builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
// Generate function 1 (out of 4).
builder.addFunction(undefined, 0 /* sig */).addBodyWithEnd([
  // signature: i_iii
  // body:
  kExprI32Const, 0x00,  // i32.const
  kExprMemoryGrow, 0x00,  // memory.grow
  kExprI32Const, 0xd3, 0xe7, 0x03,  // i32.const
  kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
  kExprI32Const, 0x84, 0x80, 0xc0, 0x05,  // i32.const
  kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
  kExprI32Const, 0x84, 0x81, 0x80, 0xc8, 0x01,  // i32.const
  kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
  kExprI32Const, 0x19,  // i32.const
  kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
  kSimdPrefix, kExprI8x16Shuffle,
  0x00, 0x00, 0x17, 0x00, 0x04, 0x04, 0x04, 0x04,
  0x04, 0x10, 0x01, 0x00, 0x04, 0x04, 0x04, 0x04,  // i8x16.shuffle
  kSimdPrefix, kExprI8x16Shuffle,
  0x04, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // i8x16.shuffle
  kSimdPrefix, kExprI8x16LeU,  // i8x16.le_u
  kSimdPrefix, kExprV8x16AnyTrue,  // v8x16.any_true
  kExprMemoryGrow, 0x00,  // memory.grow
  kExprDrop,
  kExprEnd,  // end @233
]);
builder.addExport('main', 0);
builder.instantiate();
