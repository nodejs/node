// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

load('test/mjsunit/wasm/wasm-module-builder.js');

// This is a regression test that is minimized and manually trimmed down. It
// exercises a bug in our attempt to canonicalize shuffle in platform
// independent code, see
// https://bugs.chromium.org/p/v8/issues/detail?id=11542#c6.
const builder = new WasmModuleBuilder();
builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
// Generate function 1 (out of 1).
builder.addFunction(undefined, 0 /* sig */).addBodyWithEnd([
  // signature: i_iii
  // body:
  kExprI32Const, 0x00,             // i32.const
  kSimdPrefix,   kExprI8x16Splat,  // i8x16.splat
  kExprI32Const, 0x00,             // i32.const
  kSimdPrefix,   kExprI8x16Splat,  // i8x16.splat
  kSimdPrefix,   kExprI8x16Shuffle,
  0x00,          0x15,
  0x00,          0x00,
  0x00,          0x00,
  0x00,          0x00,
  0x00,          0x00,
  0x00,          0x00,
  0x00,          0x00,
  0x00,          0x00,  // i8x16.shuffle
  kSimdPrefix,   kExprI64x2BitMask,
  0x01,      // i64x2.bitmask
  kExprEnd,  // end @30
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
assertEquals(0, instance.exports.main(1, 2, 3));
