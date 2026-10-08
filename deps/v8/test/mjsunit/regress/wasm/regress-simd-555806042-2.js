// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-revectorize --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

// Test that revectorization handles a single load feeding two *different*
// shuffle-splat patterns.
//
// 1. Load 16 bytes from offset 0 into local $val.
// 2. Splat lane 0 of $val via shuffle into local $splat0.
// 3. Splat lane 1 of $val via shuffle into local $splat1.
// 4. Two adjacent stores of $splat0 (seed 1) and two adjacent stores of
//    $splat1 (seed 2) each independently trigger 256-bit revectorization,
//    both keyed off the same shared load.
//
// Each shuffle-splat pattern gets its own PackNode, but both share the same
// underlying load. The revectorizer must emit a distinct vector
// load-transform for each pattern rather than assuming a single load feeds
// at most one shuffle pattern.

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1, false);
builder.exportMemoryAs('memory');

builder.addFunction('test', kSig_v_v)
  .addLocals(kWasmS128, 3)
  .addBody([
    // 1. Load 16 bytes from offset 0 into local 0
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalSet, 0,

    // 2. Splat lane 0 (bytes 0-3) of local 0 into local 1
    kExprLocalGet, 0,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16Shuffle,
      0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3,
    kExprLocalSet, 1,

    // 3. Splat lane 1 (bytes 4-7) of local 0 into local 2
    kExprLocalGet, 0,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16Shuffle,
      4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7,
    kExprLocalSet, 2,

    // 4. Seed 1: two adjacent 128-bit stores of local 1 (higher offset
    // first) -> revectorization seed for the first splat pattern.
    kExprI32Const, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprS128StoreMem, 0, 48,
    kExprI32Const, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprS128StoreMem, 0, 32,

    // 5. Seed 2: two adjacent 128-bit stores of local 2 (higher offset
    // first) -> revectorization seed for the second splat pattern, sharing
    // the same load as seed 1.
    kExprI32Const, 0,
    kExprLocalGet, 2,
    kSimdPrefix, kExprS128StoreMem, 0, 80,
    kExprI32Const, 0,
    kExprLocalGet, 2,
    kSimdPrefix, kExprS128StoreMem, 0, 64,
  ])
  .exportFunc();

const instance = builder.instantiate();
const memory32 = new Int32Array(instance.exports.memory.buffer);

const kLane0Value = 11;
const kLane1Value = 22;

function runAndCheck() {
  memory32[0] = kLane0Value;
  memory32[1] = kLane1Value;
  instance.exports.test();

  // Seed 1 (offsets 32 and 48) must hold lane 0, splatted.
  for (const offset of [32, 48]) {
    for (let lane = 0; lane < 4; lane++) {
      assertEquals(kLane0Value, memory32[offset / 4 + lane],
                   `offset ${offset} lane ${lane} should hold lane 0 splat`);
    }
  }

  // Seed 2 (offsets 64 and 80) must hold lane 1, splatted.
  for (const offset of [64, 80]) {
    for (let lane = 0; lane < 4; lane++) {
      assertEquals(kLane1Value, memory32[offset / 4 + lane],
                   `offset ${offset} lane ${lane} should hold lane 1 splat`);
    }
  }
}

// Baseline execution (Liftoff / interpreter before tier-up).
runAndCheck();

// Trigger Turboshaft revectorization tier-up and re-verify. Prior to the
// fix, this crashed with an UNREACHABLE hit in the Simd128Shuffle reducer,
// because only one ShufflePackNode was tracked per shared load.
%WasmTierUpFunction(instance.exports.test);
runAndCheck();
