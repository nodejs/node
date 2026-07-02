// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-revectorize --allow-natives-syntax

// Regression test for handling Simd256Extract128Lane results as function
// call arguments. On x64, the Extract128Lane node for lane 0 is elided
// during instruction selection because the XMM register aliases the lower
// 128 bits of the corresponding YMM register. When revectorization creates
// 256-bit SIMD operations and extracts lanes to pass as function arguments,
// the extracted values may reside in a spilled 256-bit stack slot. The code
// generator must handle this case when pushing arguments onto the stack.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
builder.exportMemoryAs("memory", 0);

// Create a function that takes multiple S128 arguments
const sig_idx = builder.addType(makeSig([
  kWasmS128, kWasmS128, kWasmS128, kWasmS128,
  kWasmS128, kWasmS128, kWasmS128, kWasmS128,
  kWasmS128
], [kWasmI32]));

// Callee function - just returns a constant
const target_func = builder.addFunction("target", sig_idx)
  .addBody([kExprI32Const, 42]);

// Set up an indirect function table
const table = builder.addTable(kWasmFuncRef, 1, 1);
builder.addActiveElementSegment(table.index, [kExprI32Const, 0], [target_func.index]);

// Caller function that will trigger revectorization
const caller = builder.addFunction("caller", makeSig([kWasmI32], [kWasmI32]))
  .addLocals(kWasmS128, 32);

let body = [];

// Load 16 pairs of adjacent S128 values from memory
// These will be revectorized into S256 operations
for (let i = 0; i < 16; i++) {
  const local_lo = 1 + i * 2;
  const local_hi = 2 + i * 2;
  const offset_lo = i * 32;
  const offset_hi = i * 32 + 16;

  // Load and process first S128
  body.push(
    kExprLocalGet, 0,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128LoadMem), 0, ...wasmUnsignedLeb(offset_lo),
    kExprI32Const, 1,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Splat),
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Add),
    kExprLocalSet, local_lo
  );

  // Load and process second S128
  body.push(
    kExprLocalGet, 0,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128LoadMem), 0, ...wasmUnsignedLeb(offset_hi),
    kExprI32Const, 1,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Splat),
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Add),
    kExprLocalSet, local_hi
  );
}

// Call function indirectly with 9 S128 arguments
// These arguments come from revectorized 256-bit operations
// and their extracted lanes may be read from 256-bit stack slots.
body.push(
  kExprLocalGet, 3,   // pair1_lo
  kExprLocalGet, 5,   // pair2_lo
  kExprLocalGet, 7,   // pair3_lo
  kExprLocalGet, 9,   // pair4_lo
  kExprLocalGet, 11,  // pair5_lo
  kExprLocalGet, 13,  // pair6_lo
  kExprLocalGet, 15,  // pair7_lo
  kExprLocalGet, 2,   // pair0_hi
  kExprLocalGet, 1,   // pair0_lo
  kExprI32Const, 0,   // table index
  kExprCallIndirect, ...wasmUnsignedLeb(sig_idx), 0,
  kExprDrop
);

// Store all pairs back to memory after the call.
// This keeps all revectorized 256-bit values live across the call boundary,
// forcing the register allocator to spill them to 256-bit stack slots.
for (let i = 0; i < 16; i++) {
  const local_lo = 1 + i * 2;
  const local_hi = 2 + i * 2;
  const offset_lo = i * 32;
  const offset_hi = i * 32 + 16;
  body.push(
    kExprLocalGet, 0,
    kExprLocalGet, local_lo,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128StoreMem), 0, ...wasmUnsignedLeb(offset_lo),
    kExprLocalGet, 0,
    kExprLocalGet, local_hi,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128StoreMem), 0, ...wasmUnsignedLeb(offset_hi)
  );
}

body.push(kExprI32Const, 42);
caller.addBody(body).exportFunc();

const instance = builder.instantiate();
// Initialize memory with some data
const mem = new Uint8Array(instance.exports.memory.buffer);
for (let i = 0; i < 512; i++) {
  mem[i] = i & 0xff;
}

// Force tier-up to the optimizing compiler where revectorization runs.
%WasmTierUpFunction(instance.exports.caller);

// Compute expected values before the call overwrites memory.
// Each i32 lane = original_i32_value + 1.
const view = new DataView(instance.exports.memory.buffer);
const expected = new Int32Array(128);  // 16 pairs * 4 lanes * 2 = 128 i32s
for (let i = 0; i < 128; i++) {
  expected[i] = view.getInt32(i * 4, true) + 1;
}

// This should not crash with a DCHECK failure
const result = instance.exports.caller(0);
assertEquals(42, result);

// Verify the stored values are correct.
for (let i = 0; i < 128; i++) {
  assertEquals(expected[i], view.getInt32(i * 4, true));
}
