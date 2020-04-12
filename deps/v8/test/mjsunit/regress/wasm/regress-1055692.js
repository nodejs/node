// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging --wasm-interpret-all --experimental-wasm-simd

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false);
builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
// Generate function 1 (out of 1).
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
// signature: i_iii
// body:
kExprI32Const, 0x75,  // i32.const
kExprI32Const, 0x74,  // i32.const
kExprI32Const, 0x18,  // i32.const
kSimdPrefix, kExprS8x16LoadSplat,  // s8x16.load_splat
kExprUnreachable,  // unreachable
kExprUnreachable,  // unreachable
kExprI32Const, 0x6f,  // i32.const
kExprI32Const, 0x7f,  // i32.const
kExprI32Const, 0x6f,  // i32.const
kExprDrop,
kExprDrop,
kExprDrop,
kExprDrop,
kExprDrop,
kExprEnd,  // end @18
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
print(instance.exports.main(1, 2, 3));
