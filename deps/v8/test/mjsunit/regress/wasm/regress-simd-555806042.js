// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-revectorize --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

// Test that revectorization of a SIMD shuffle-splat into Simd256LoadTransform
// does not reorder the memory load past an intervening store (RAW hazard).
//
// 1. Load 16 bytes from offset 0 into local $val (initial memory value: 42).
// 2. Intervening store writes 99 into offset 0.
// 3. Splat lane 0 of $val via shuffle (expected: 42 in all lanes).
// 4. Two adjacent stores of the splat result trigger 256-bit revectorization.
//
// If revectorization delays the load to the shuffle site, it will read 99
// instead of 42.

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1, false);
builder.exportMemoryAs('memory');

builder.addFunction('test', kSig_v_v)
  .addLocals(kWasmS128, 2)
  .addBody([
    // 1. Load 16 bytes from offset 0 into local 1
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalSet, 1,

    // 2. Intervening store: write 99 to memory[0]
    kExprI32Const, 0,
    kExprI32Const, ...wasmSignedLeb(99),
    kExprI32StoreMem, 0, 0,

    // 3. Splat lane 0 of local 1 across all lanes of local 0
    kExprLocalGet, 1,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Shuffle,
      0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3,
    kExprLocalSet, 0,

    // 4. Two adjacent 128-bit stores to offsets 32 and 48 -> revectorization seed
    kExprI32Const, 0,
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128StoreMem, 0, 48,

    kExprI32Const, 0,
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128StoreMem, 0, 32,
  ])
  .exportFunc();

const instance = builder.instantiate();
const memory = new Int32Array(instance.exports.memory.buffer);

const kOriginalValue = 42;
const kNewValue = 99;

// Baseline execution (Liftoff / interpreter before tier-up)
memory[0] = kOriginalValue;
instance.exports.test();
assertEquals(kOriginalValue, memory[32 / 4],
             'Baseline: splat must hold original value before store');

// Reset memory and trigger Turboshaft revectorization tier-up
memory[0] = kOriginalValue;
%WasmTierUpFunction(instance.exports.test);
instance.exports.test();

// Under buggy revectorization, memory[32 / 4] gets populated with kNewValue (99)
// instead of kOriginalValue (42).
assertEquals(kOriginalValue, memory[32 / 4],
             'Optimized: memory load must NOT be reordered past intervening store');
