// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addType(makeSig([], [kWasmS128, kWasmF64, kWasmS128, kWasmF64, kWasmF64, kWasmF32, kWasmF64, kWasmS128, kWasmF32]));
builder.addFunction('foo', kSig_v_v)
  .addBody([
kExprBlock, /* sig */ 0,                                          // block
  kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const
  kExprI32Const, 0x00,                                            // i32.const
  kSimdPrefix, kExprI8x16Splat,                                   // i8x16.splat
  kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const
  kExprI32Const, 0x00,                                            // i32.const
  kSimdPrefix, kExprI8x16Splat,                                   // i8x16.splat
  kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const
  kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const
  kExprF32Const, 0x00, 0x00, 0x00, 0x00,                          // f32.const
  kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const
  kExprI32Const, 0x00,                                            // i32.const
  kSimdPrefix, kExprI8x16Splat,                                   // i8x16.splat
  kExprF32Const, 0x00, 0x00, 0x00, 0x00,                          // f32.const
  kExprBr, 0,                                                     // br depth=0
  kExprUnreachable,                                               // unreachable
  kExprEnd,                                                       // end
kExprUnreachable,                                                 // unreachable
]);
builder.toModule();
