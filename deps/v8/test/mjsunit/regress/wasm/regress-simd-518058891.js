// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-revectorize --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Regression test for https://crbug.com/518058891
// Signed int32 overflow in splat-shuffle offset fold leads to OOB in wasm revectorizer.
//
// The bug was in src/compiler/turboshaft/wasm-revec-reducer.h where the
// reducer computed `const int offset = splat_index + load.offset`.
// When load.offset is close to INT32_MAX (e.g., 0x7FFFFFF4) and splat_index
// is 12 (for lane 3 of a 4-byte element), the addition overflows to INT32_MIN,
// causing the computed base address to wrap around to ~2GB before the WASM
// linear memory start, where there is no guard region.
//
// This test exercises two failure modes:
// 1. UBSAN detects the signed integer overflow during compilation
// 2. Runtime miscompilation causes reads from incorrect memory addresses

const kOffset = 0x7FFFFFF4;  // INT32_MAX - 11; 12 + kOffset wraps to INT32_MIN

function buildModule(initial_pages) {
  const builder = new WasmModuleBuilder();
  // max=65536 pages → 4GiB max, so decoder's static OOB check passes
  builder.addMemory(initial_pages, 65536);
  builder.exportMemoryAs('mem');

  builder.addFunction('test_revec', kSig_v_ii)
    .addLocals(kWasmS128, 1)  // local 2: shuffle result
    .addBody([
      // Load from high offset that can trigger overflow
      kExprLocalGet, 0,
      kSimdPrefix, kExprS128LoadMem, 0, ...wasmUnsignedLeb(kOffset),

      // Right operand for shuffle (any non-Load op)
      kExprI32Const, 0,
      kSimdPrefix, kExprI8x16Splat,

      // Shuffle: splat lane 3 of LEFT input (bytes [12,13,14,15] repeated)
      // This triggers TryMatchSplat<4> with index=3
      kSimdPrefix, kExprI8x16Shuffle,
        12, 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15,
      kExprLocalSet, 2,

      // Two adjacent v128.store of the same value creates a store-seed
      // for revectorization. Emit higher-offset store first so
      // DecideVectorize() detects the pattern.
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kSimdPrefix, kExprS128StoreMem, 0, 16,

      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kSimdPrefix, kExprS128StoreMem, 0, 0,
    ])
    .exportFunc();

  const instance = builder.instantiate();
  // Force Turboshaft optimizing compiler (where revectorizer runs)
  %WasmTierUpFunction(instance.exports.test_revec);
  return instance;
}

// Test Part 1: Compilation should not trigger integer overflow
// (Previously would trigger UBSAN error during tier-up)
(function testCompilationWithOverflowRisk() {
  print('Testing compilation with potential overflow offset...');
  const instance = buildModule(1);
  print('  ✓ Compilation successful (no integer overflow)');
})();

// Test Part 2: Runtime correctness with large memory
// Ensure the fix prevents miscompilation when memory is large enough
(function testRuntimeCorrectness() {
  print('Testing runtime correctness with large memory...');

  let instance;
  try {
    // Allocate 2GiB+ memory: 32769 pages × 64KiB = 0x80010000 bytes
    instance = buildModule(32769);
  } catch (e) {
    print('  ⚠ Skipping runtime test: cannot allocate 2GiB memory: ' + e);
    return;
  }

  const f = instance.exports.test_revec;
  const mem = new DataView(instance.exports.mem.buffer);

  // Set ground-truth value at the address that lane 3 should read from
  // idx=0, offset=0x7FFFFFF4 → effective address for lane 3 = 0x80000000
  mem.setUint32(0x80000000, 0xDEADBEEF, true);

  // Execute: should store 0xDEADBEEF repeated 8 times at mem[64..96)
  try {
    f(0, 64);

    // Verify the broadcast was correct
    const got = mem.getUint32(64, true) >>> 0;
    assertEquals(0xDEADBEEF, got,
                 'Should broadcast correct value from mem[0x80000000]');

    // Verify all 8 lanes contain the same value
    for (let i = 0; i < 8; i++) {
      const val = mem.getUint32(64 + i * 4, true) >>> 0;
      assertEquals(0xDEADBEEF, val,
                   'Lane ' + i + ' should contain 0xDEADBEEF');
    }

    print('  ✓ Runtime correctness verified');
  } catch (e) {
    // The bug would cause either:
    // - A fault when trying to read from (mem_start - 0x80000000)
    // - Or incorrect values being broadcast
    throw new Error('Unexpected error during execution: ' + e.message +
                    '\nThis may indicate the overflow fix did not work correctly.');
  }
})();

print('All tests passed!');
