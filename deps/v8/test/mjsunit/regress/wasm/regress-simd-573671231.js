// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-revectorize --allow-natives-syntax

// Regression test for MoveOptimizer::FinalizeMoves zeroing the upper 128
// bits of a Simd256 register. A 128-bit kSimd128 value shares the same
// virtual register as its 256-bit kSimd256 producer (e.g. a lane-0
// "extract" that got elided and aliased to its Simd256 producer on x64).
// When that virtual register is spilled to a 256-bit stack slot and later
// reloaded in the same ParallelMove by both a 128-bit destination and a
// 256-bit destination, LoadCompare's sort didn't distinguish Simd256 from
// Simd128 operand widths. This let MoveOptimizer::FinalizeMoves pick the
// 128-bit reload as the group's representative ("group_begin") and rewrite
// the 256-bit reload to source from the 128-bit register instead of the
// stack slot, silently zeroing the upper 128 bits (lanes 4..7) of the wider
// register.
//
// `w0`/`w1` are revectorized into one 256-bit value, which gets spilled
// across the call to the imported `nop` function. After the call, `phi_128`
// reloads only the low 128 bits (w0), while `phi_low`/`phi_high` need the
// full spilled value (w0 and w1) to feed a revectorized store -- both
// reloading from the very same spill slot in the same gap.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestMoveOptimizerSimd256ZeroHigh128() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);
  builder.exportMemoryAs('memory');
  const call_import = builder.addImport('env', 'nop', makeSig([], []));
  builder.addFunction('test', makeSig([kWasmI32, kWasmI32], []))
    .addLocals(kWasmS128, 5)  // locals 2..6: w0, w1, phi_128, phi_low, phi_high
    .addBody([
      kExprLocalGet, 0,
      kExprIf, kWasmVoid,
        // Load v0, v1 from addr+0, addr+16 and add to themselves -> w0, w1
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, ...wasmUnsignedLeb(0),
        kExprLocalTee, 2,
        kExprLocalGet, 2,
        kSimdPrefix, ...wasmSignedLeb(kExprI32x4Add),
        kExprLocalSet, 2,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, ...wasmUnsignedLeb(16),
        kExprLocalTee, 3,
        kExprLocalGet, 3,
        kSimdPrefix, ...wasmSignedLeb(kExprI32x4Add),
        kExprLocalSet, 3,
        // Call imported nop() so v256 = [w0, w1] is spilled to a 256-bit
        // stack slot.
        kExprCallFunction, call_import,
        // phi_128 (local 4) = w0 (the elided, lane-0 "extract" of v256).
        kExprLocalGet, 2,
        kExprLocalSet, 4,
        // phi_low (local 5) = w0, phi_high (local 6) = w1.
        kExprLocalGet, 2,
        kExprLocalSet, 5,
        kExprLocalGet, 3,
        kExprLocalSet, 6,
      kExprElse,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, ...wasmUnsignedLeb(32),
        kExprLocalSet, 4,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, ...wasmUnsignedLeb(64),
        kExprLocalSet, 5,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, ...wasmUnsignedLeb(80),
        kExprLocalSet, 6,
      kExprEnd,
      // Store phi_low and phi_high to addr+128 and addr+144 (revectorized).
      kExprLocalGet, 1,
      kExprLocalGet, 5,
      kSimdPrefix, kExprS128StoreMem, 0, ...wasmUnsignedLeb(128),
      kExprLocalGet, 1,
      kExprLocalGet, 6,
      kSimdPrefix, kExprS128StoreMem, 0, ...wasmUnsignedLeb(144),
      // Store phi_128 (the 128-bit use) to addr+200.
      kExprLocalGet, 1,
      kExprLocalGet, 4,
      kSimdPrefix, kExprS128StoreMem, 0, ...wasmUnsignedLeb(200),
    ])
    .exportFunc();
  const instance = builder.instantiate({env: {nop: () => {}}});
  const i32 = new Int32Array(instance.exports.memory.buffer);
  for (let i = 0; i < 8; i++) {
    i32[i] = (i + 1) * 10;  // lanes 0..3 = 10..40, lanes 4..7 = 50..80
  }
  // Run once on Liftoff first.
  instance.exports.test(1, 0);

  // Force tier-up to the optimizing compiler where revectorization runs.
  %WasmTierUpFunction(instance.exports.test);
  instance.exports.test(1, 0);

  const out = Array.from(i32.slice(128 / 4, 160 / 4));
  const expected = [20, 40, 60, 80, 100, 120, 140, 160];
  assertEquals(expected, out);
})();
