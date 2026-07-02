// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-fp16

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// This test verifies that f16x8 relational operations correctly compare and
// pack all 8 lanes. The bug (499027764) was that on x64, only the lower 4 lanes
// were being correctly packed due to incorrect use of vpackssdw on YMM registers.

function RunTest() {
  const builder = new WasmModuleBuilder();
  const res = builder.addGlobal(kWasmS128, true);
  builder.addFunction("test_eq", kSig_v_v)
    .addLocals(kWasmS128, 2)
    .addBody([
      // Local 0: [0, 1, 2, 3, 4, 5, 6, 7] in F16
      ...wasmF32Const(0), kSimdPrefix, ...kExprF16x8Splat,
      ...wasmF32Const(1), kSimdPrefix, ...kExprF16x8ReplaceLane, 1,
      ...wasmF32Const(2), kSimdPrefix, ...kExprF16x8ReplaceLane, 2,
      ...wasmF32Const(3), kSimdPrefix, ...kExprF16x8ReplaceLane, 3,
      ...wasmF32Const(4), kSimdPrefix, ...kExprF16x8ReplaceLane, 4,
      ...wasmF32Const(5), kSimdPrefix, ...kExprF16x8ReplaceLane, 5,
      ...wasmF32Const(6), kSimdPrefix, ...kExprF16x8ReplaceLane, 6,
      ...wasmF32Const(7), kSimdPrefix, ...kExprF16x8ReplaceLane, 7,
      kExprLocalSet, 0,

      // Local 1: [0, 1, 2, 3, 0, 0, 0, 0] in F16
      ...wasmF32Const(0), kSimdPrefix, ...kExprF16x8Splat,
      ...wasmF32Const(1), kSimdPrefix, ...kExprF16x8ReplaceLane, 1,
      ...wasmF32Const(2), kSimdPrefix, ...kExprF16x8ReplaceLane, 2,
      ...wasmF32Const(3), kSimdPrefix, ...kExprF16x8ReplaceLane, 3,
      kExprLocalSet, 1,

      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kSimdPrefix, ...kExprF16x8Eq,
      kExprGlobalSet, res.index,
    ]).exportFunc();

  builder.addFunction("test_lt", kSig_v_v)
    .addLocals(kWasmS128, 2)
    .addBody([
      // Local 0: [0, 1, 2, 3, 4, 5, 6, 7]
      ...wasmF32Const(0), kSimdPrefix, ...kExprF16x8Splat,
      ...wasmF32Const(1), kSimdPrefix, ...kExprF16x8ReplaceLane, 1,
      ...wasmF32Const(2), kSimdPrefix, ...kExprF16x8ReplaceLane, 2,
      ...wasmF32Const(3), kSimdPrefix, ...kExprF16x8ReplaceLane, 3,
      ...wasmF32Const(4), kSimdPrefix, ...kExprF16x8ReplaceLane, 4,
      ...wasmF32Const(5), kSimdPrefix, ...kExprF16x8ReplaceLane, 5,
      ...wasmF32Const(6), kSimdPrefix, ...kExprF16x8ReplaceLane, 6,
      ...wasmF32Const(7), kSimdPrefix, ...kExprF16x8ReplaceLane, 7,
      kExprLocalSet, 0,

      // Local 1: [1, 2, 3, 4, 5, 6, 7, 8]
      ...wasmF32Const(1), kSimdPrefix, ...kExprF16x8Splat,
      ...wasmF32Const(2), kSimdPrefix, ...kExprF16x8ReplaceLane, 1,
      ...wasmF32Const(3), kSimdPrefix, ...kExprF16x8ReplaceLane, 2,
      ...wasmF32Const(4), kSimdPrefix, ...kExprF16x8ReplaceLane, 3,
      ...wasmF32Const(5), kSimdPrefix, ...kExprF16x8ReplaceLane, 4,
      ...wasmF32Const(6), kSimdPrefix, ...kExprF16x8ReplaceLane, 5,
      ...wasmF32Const(7), kSimdPrefix, ...kExprF16x8ReplaceLane, 6,
      ...wasmF32Const(8), kSimdPrefix, ...kExprF16x8ReplaceLane, 7,
      kExprLocalSet, 1,

      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kSimdPrefix, ...kExprF16x8Lt,
      kExprGlobalSet, res.index,
    ]).exportFunc();

  for (let i = 0; i < 8; ++i) {
    builder.addFunction(`get_lane_${i}`, kSig_i_v)
      .addBody([
        kExprGlobalGet, res.index,
        kSimdPrefix, kExprI16x8ExtractLaneS, i,
      ]).exportFunc();
  }

  const instance = builder.instantiate();
  instance.exports.test_eq();
  for (let i = 0; i < 4; ++i) {
    assertEquals(-1, instance.exports[`get_lane_${i}`](), `eq lane ${i}`);
  }
  for (let i = 4; i < 8; ++i) {
    assertEquals(0, instance.exports[`get_lane_${i}`](), `eq lane ${i}`);
  }
  instance.exports.test_lt();
  for (let i = 0; i < 8; ++i) {
    assertEquals(-1, instance.exports[`get_lane_${i}`](), `lt lane ${i}`);
  }
}

RunTest();
