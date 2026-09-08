// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-revectorize --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

// Regression test: an unmasked splat index in wasm-revec-reducer.h caused an
// out-of-bounds memory read when revectorizing a SIMD splat that reads lane 0
// of the *second* input of a shuffle. The second input's load already points
// at the right 128-bit vector, but the unmasked index (4 for 32-bit lanes)
// added an extra 16 bytes to the address, reading past the end of memory.
//
// Two adjacent 16-byte stores (offsets 32 and 48) form the store seed that
// triggers revectorization; both consume the same shuffle-splat result, so the
// splat gets rewritten into a 256-bit load-transform where the bug lived.

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1, false);
builder.exportMemoryAs('memory');

builder.addFunction('test', kSig_v_v)
  .addLocals(kWasmS128, 1)
  .addBody([
    // Load 16 bytes from offset 0 (first shuffle input).
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,

    // Load 16 bytes from offset 65520 -- the final 16 bytes of the single
    // memory page (second shuffle input).
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, ...wasmUnsignedLeb(65520),

    // Splat lane 0 of the second input (overall 32-bit lane index 4) across all
    // lanes. Bytes 16..19 of the combined 32-byte shuffle map to the first
    // 4 bytes of the second input, i.e. memory[65520..65523].
    kSimdPrefix, kExprI8x16Shuffle,
      16, 17, 18, 19, 16, 17, 18, 19, 16, 17, 18, 19, 16, 17, 18, 19,

    kExprLocalSet, 0,

    // Two adjacent stores of the splat result -> revectorization seed.
    kExprI32Const, 0,
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128StoreMem, 0, 48,

    kExprI32Const, 0,
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128StoreMem, 0, 32,
  ])
  .exportFunc();

const instance = builder.instantiate();
const memory = new Uint8Array(instance.exports.memory.buffer);

// Distinct source bytes so an off-by-16 read (which would land on the trailing
// zero-filled bytes / out of bounds) produces a detectable mismatch.
const kSource = [0xde, 0xad, 0xbe, 0xef];
memory.set(kSource, 65520);

// Execute baseline version first.
instance.exports.test();

// Tier up the function to trigger revectorization optimization.
%WasmTierUpFunction(instance.exports.test);
// This must not read out of bounds while revectorizing the splat.
instance.exports.test();

// The 32-bit lane at memory[65520..65523] must be splatted across the whole
// 32-byte destination region [32, 64).
for (let i = 32; i < 64; i++) {
  assertEquals(kSource[i % 4], memory[i],
               `byte at offset ${i} should be the splatted source lane`);
}
