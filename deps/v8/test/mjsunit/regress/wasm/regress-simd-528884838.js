// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-revectorize --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Regression test for https://crbug.com/528884838
// Missing CpuFeatureScope(AVX) in CodeGenerator::AssembleSwap for Simd256
// stack slots.
//
// When the register allocator needs to swap two 256-bit SIMD values that have
// been spilled to stack slots, it emits vmovups + vxorps (AVX instructions)
// via the XOR-swap trick. The CpuFeatureScope guard was missing, which is
// required by the assembler to emit AVX-encoded instructions.
//
// This test exercises the path by creating enough SIMD register pressure
// (60 S128 locals → 30 Simd256 locals after revectorization) that the
// register allocator is forced to spill Simd256 values and generate
// AssembleSwap for Simd256 stack slots.
//
// The function:
//   1. Loads 15 lo/hi S128 pairs from mem[base + i*32] and adds 1 to each
//      i32x4 lane, storing results in locals 1..30.
//   2. Loops once: stores locals 1..30 back to memory at the same offsets,
//      stores zero-initialized locals 31..60 at large offsets, then swaps
//      all 30 pairs of locals to maximise live-range interference.
//
// After the call the test verifies:
//   - mem[0..480)        : each i32 equals original value + 1
//   - mem[1024..30*1024) : each i32 equals 0  (locals 31..60 are zero-initialized)

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
builder.exportMemoryAs('mem');

// "caller" has 60 S128 locals (30 lo/hi pairs) + 1 I32 loop counter.
const caller = builder.addFunction('caller', makeSig([kWasmI32], [kWasmI32]))
  .addLocals(kWasmS128, 60)  // locals 1..60
  .addLocals(kWasmI32, 1);   // local 61: loop counter

let body = [];

// Phase 1: load 15 lo/hi pairs from memory and increment each i32x4 lane by 1.
// All 30 loads share the same base pointer, creating high register pressure.
for (let i = 0; i < 15; i++) {
  const local_lo = 1 + i * 2;
  const local_hi = 2 + i * 2;
  const offset_lo = i * 32;
  const offset_hi = i * 32 + 16;
  body.push(
    kExprLocalGet, 0,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128LoadMem), 0,
    ...wasmUnsignedLeb(offset_lo),
    kExprI32Const, 1,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Splat),
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Add),
    kExprLocalSet, local_lo
  );
  body.push(
    kExprLocalGet, 0,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128LoadMem), 0,
    ...wasmUnsignedLeb(offset_hi),
    kExprI32Const, 1,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Splat),
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Add),
    kExprLocalSet, local_hi
  );
}

// Set loop counter to 1 (single pass).
body.push(
  kExprI32Const, 1,
  kExprLocalSet, 61
);

body.push(kExprLoop, kWasmVoid);

// Store 15 lo/hi pairs at adjacent 16-byte-aligned offsets.
// The adjacent pair pattern triggers revectorization into 256-bit stores.
// With 30 Simd256 values under register pressure, the allocator must spill
// and generates AssembleSwap for Simd256 stack slots — the bug site.
for (let i = 0; i < 15; i++) {
  const local_lo = 1 + i * 2;
  const local_hi = 2 + i * 2;
  const offset_lo = i * 32;
  const offset_hi = i * 32 + 16;
  body.push(
    kExprLocalGet, 0, kExprLocalGet, local_lo,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128StoreMem), 0,
    ...wasmUnsignedLeb(offset_lo),
    kExprLocalGet, 0, kExprLocalGet, local_hi,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128StoreMem), 0,
    ...wasmUnsignedLeb(offset_hi)
  );
}

// Store locals 31..60 (zero-initialized) at large offsets to broaden
// register pressure and verify zero output after the call.
for (let i = 0; i < 30; i++) {
  const local = 31 + i;
  const offset = 1024 + i * 1024;
  body.push(
    kExprLocalGet, 0, kExprLocalGet, local,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128StoreMem), 0,
    ...wasmUnsignedLeb(offset)
  );
}

// Swap 30 pairs of locals (local_a <-> local_b).
// These live-range interference patterns force the register allocator to
// emit AssembleSwap for spilled Simd256 slots.
for (let i = 0; i < 30; i++) {
  const local_a = 1 + i;
  const local_b = 31 + i;
  body.push(
    kExprLocalGet, local_a,
    kExprLocalGet, local_b,
    kExprLocalSet, local_a,
    kExprLocalSet, local_b
  );
}

// Decrement counter and loop back if still > 0.
body.push(
  kExprLocalGet, 61,
  kExprI32Const, 1,
  kExprI32Sub,
  kExprLocalTee, 61,
  kExprBrIf, 0,
  kExprEnd
);

body.push(kExprI32Const, 42);
caller.addBody(body).exportFunc();

const instance = builder.instantiate();
const view = new DataView(instance.exports.mem.buffer);

// Initialize memory: fill first 512 bytes with a recognizable pattern so
// that the +1 increment is visible and verifiable.
for (let i = 0; i < 512; i++) {
  view.setUint8(i, i & 0xff);
}

// Capture expected values (original i32 + 1) before the call overwrites memory.
// The function reads 30 x 16 = 480 bytes starting at base=0.
const kNumI32s = 120;  // 480 bytes / 4
const expected = new Int32Array(kNumI32s);
for (let i = 0; i < kNumI32s; i++) {
  expected[i] = (view.getInt32(i * 4, true) + 1) | 0;
}

// Without the fix, tier-up crashes due to the missing CpuFeatureScope(AVX)
// in AssembleSwap when generating the XOR-swap for Simd256 stack slots.
%WasmTierUpFunction(instance.exports.caller);

const result = instance.exports.caller(0);
assertEquals(42, result);

// Verify locals 1..30 were stored correctly (each i32 = original + 1).
for (let i = 0; i < kNumI32s; i++) {
  assertEquals(expected[i], view.getInt32(i * 4, true),
               'i32 at offset ' + (i * 4));
}

// Verify locals 31..60 (zero-initialized) were stored at large offsets.
for (let i = 0; i < 30; i++) {
  const base = 1024 + i * 1024;
  for (let lane = 0; lane < 4; lane++) {
    assertEquals(0, view.getInt32(base + lane * 4, true),
                 'zero local ' + i + ' lane ' + lane);
  }
}
