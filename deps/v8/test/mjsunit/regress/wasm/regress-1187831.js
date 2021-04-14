// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false, true);
builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
builder.addType(makeSig([], []));
builder.setTableBounds(1, 1);
builder.addElementSegment(0, 0, false, [0]);
// Generate function 1 (out of 1).
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
// signature: i_iii
// body:
kExprI32Const, 0x03,  // i32.const
kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
kExprI32Const, 0x00,  // i32.const
kSimdPrefix, kExprI8x16ReplaceLane, 0x00,  // i8x16.replace_lane
kSimdPrefix, kExprI32x4ExtAddPairwiseI16x8U,  // i32x4.extadd_pairwise_i16x8_u
kSimdPrefix, kExprI8x16ExtractLaneU, 0x00,  // i8x16.extract_lane_u
kExprEnd,  // end @15
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
assertEquals(3, instance.exports.main(1, 2, 3));
