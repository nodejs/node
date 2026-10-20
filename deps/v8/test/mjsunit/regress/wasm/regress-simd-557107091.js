// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-revectorize --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Regression test for https://crbug.com/557107091
//
// TryMatchExtendIntToF32x4 in src/compiler/turboshaft/wasm-revec-reducer.cc
// compared the two halves' start lanes using min/max, which discards which of
// node_group[0] / node_group[1] extends from the lower four lanes.
//
// That order matters: the group is revectorized into a single in-order 256-bit
// widening, and GetExtractOpIfNeeded picks the half by the node's position in
// the pack node group, so node_group[0] always receives lanes start..start+3.
//
// Here the store to offset 16 takes lanes 4..7 and the store to offset 32
// takes lanes 0..3, but the stores are packed in address order. The reversed
// pair was accepted with start_lane pinned to the minimum, so each store
// received the other store's half.

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
builder.exportMemoryAs('memory', 0);

function make_extract_convert_f32x4(local_idx, start_lane) {
  let insts = [];

  // lane 0: f32x4.splat(f32.convert_i32_s(i16x8.extract_lane_s(start_lane)))
  insts.push(kExprLocalGet, local_idx);
  insts.push(kSimdPrefix, kExprI16x8ExtractLaneS, start_lane);
  insts.push(kExprF32SConvertI32);
  insts.push(kSimdPrefix, kExprF32x4Splat);

  for (let lane = 1; lane < 4; lane++) {
    insts.push(kExprLocalGet, local_idx);
    insts.push(kSimdPrefix, kExprI16x8ExtractLaneS, start_lane + lane);
    insts.push(kExprF32SConvertI32);
    insts.push(kSimdPrefix, kExprF32x4ReplaceLane, lane);
  }

  return insts;
}

builder.addFunction('test', kSig_v_v).addLocals(kWasmS128, 1)
  .addBody([
    // local 0 = i16x8 loaded from offset 0.
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 4, 0,
    kExprLocalSet, 0,

    // Store to the lower address takes the *upper* four lanes ...
    kExprI32Const, 0,
    ...make_extract_convert_f32x4(0, 4),
    kSimdPrefix, kExprS128StoreMem, 4, 16,

    // ... and the store to the higher address takes the lower four lanes.
    kExprI32Const, 0,
    ...make_extract_convert_f32x4(0, 0),
    kSimdPrefix, kExprS128StoreMem, 4, 32,
  ]).exportFunc();

const instance = builder.instantiate();
const mem = new DataView(instance.exports.memory.buffer);

for (let i = 0; i < 8; i++) mem.setInt16(2 * i, 10 * (i + 1), true);

instance.exports.test();

// Offset 16 holds lanes 4..7. The bug stored lanes 0..3 here instead.
assertEquals(50, mem.getFloat32(16, true));
assertEquals(60, mem.getFloat32(20, true));
assertEquals(70, mem.getFloat32(24, true));
assertEquals(80, mem.getFloat32(28, true));

// Offset 32 holds lanes 0..3.
assertEquals(10, mem.getFloat32(32, true));
assertEquals(20, mem.getFloat32(36, true));
assertEquals(30, mem.getFloat32(40, true));
assertEquals(40, mem.getFloat32(44, true));
