// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-simd

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false);
builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
// Generate function 1 (out of 1).
builder.addFunction(undefined, 0 /* sig */).addBodyWithEnd([
  // signature: i_iii
  // body:
  kExprI32Const, 0xba, 0x01,       // i32.const
  kSimdPrefix, kExprI16x8Splat,    // i16x8.splat
  kExprMemorySize, 0x00,           // memory.size
  kSimdPrefix, kExprI16x8ShrS,     // i16x8.shr_s
  kSimdPrefix, kExprV8x16AnyTrue,  // v8x16.any_true
  kExprMemorySize, 0x00,           // memory.size
  kExprI32RemS,                    // i32.rem_s
  kExprEnd,                        // end @15
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
instance.exports.main(1, 2, 3);
