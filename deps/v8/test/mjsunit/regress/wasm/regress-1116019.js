// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-simd

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false);
builder.addType(makeSig([], [kWasmI32]));
// Generate function 1 (out of 1).
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
// signature: i_v
// body:
kExprI32Const, 0x00,  // i32.const
kSimdPrefix, kExprS128Load8x8U, 0x03, 0xff, 0xff, 0x3f,  // i16x8.load8x8_u
kSimdPrefix, kExprI16x8ExtractLaneS, 0,
kExprEnd,  // end @371
]).exportAs('main');
const instance = builder.instantiate();
assertTraps(kTrapMemOutOfBounds, () => instance.exports.main());
