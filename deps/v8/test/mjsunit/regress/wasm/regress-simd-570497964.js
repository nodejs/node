// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-revectorize --wasm-assert-types --allow-natives-syntax
// Flags: --no-wasm-inlining
//
// --no-wasm-inlining prevents `inner` from being inlined into `test` after
// the Liftoff warm-up call below (wasm inlining uses call-count feedback
// collected by Liftoff), which would otherwise eliminate the cross-function
// Return ABI boundary this test depends on.

// Regression test for returning a revectorized Simd256Extract128Lane(lane=0)
// result as one of several wasm multi-return values. On x64, the lane-0
// extraction is elided during instruction selection: the result is aliased
// (via EmitIdentity) directly to its Simd256 producer, so the register
// allocator ends up treating the value as a full 256-bit (32-byte) quantity.
// When this value has to be placed in a fixed caller-frame stack slot sized
// for a 128-bit (16-byte) wasm v128 value, the generated move wrote the full
// 32 bytes into that 16-byte slot, clobbering whatever memory follows it.
//
// Moves for multi-return values are emitted in return-index order, and their
// stack slot addresses increase monotonically with index. So a buggy 32-byte
// write at slot i always overflows into slot i+1 -- but slot i+1's own move
// always executes right after and overwrites/heals that overflow, UNLESS i
// is the very last return value, in which case the overflow spills past the
// whole return area into whatever (unclaimed) stack memory follows it.
//
// `inner` returns [f64, f64, v128, i64, v128]: the two f64 returns occupy
// both available FP/SIMD return registers (xmm1/xmm2) on x64, forcing the
// v128 returns onto the stack; the i64 return goes into a GP register. The
// same elided extraction is returned twice so that the second occurrence is
// the *last* return value -- the only position where its overflow isn't
// healed by a subsequent return value's own move, making the corruption
// observable.
//
// To observe that overflow, `test` (the caller) keeps an unrelated v128
// value (`extra`) live across the call to `inner`, so it gets spilled to
// `test`'s own frame, adjacent to `inner`'s outgoing return-value area.
// Without the fix, the overflow clobbers `extra`'s spill slot with unrelated
// data (the packed op's other lane); with the fix, it reads back correctly.
// `inner`'s own return values aren't otherwise needed, so they're dropped.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
builder.exportMemoryAs('memory');

const inner_sig = makeSig(
    [kWasmI32],
    [kWasmF64, kWasmF64, kWasmS128, kWasmI64, kWasmS128]);

const inner = builder.addFunction('inner', inner_sig)
  .addLocals(kWasmS128, 2)  // 1: lo, 2: hi
  .addBody([
    // lo = i32x4.add(v128.load(offset + 0), i32x4.splat(1))
    kExprLocalGet, 0,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128LoadMem), 0, 0,
    kExprI32Const, 1,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Splat),
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Add),
    kExprLocalSet, 1,

    // hi = i32x4.add(v128.load(offset + 16), i32x4.splat(1))
    kExprLocalGet, 0,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128LoadMem), 0, 16,
    kExprI32Const, 1,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Splat),
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Add),
    kExprLocalSet, 2,

    // Combine lo and hi with an i32x4.add "reduce seed" in revectorizer.
    // `lo` is also used below as a return value (an "external use"
    // outside the packed region), which is exactly what
    // produces the elided Simd256Extract128Lane(lane=0) this test targets.
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Add),
    kExprDrop,

    // Return values, in signature order: f64, f64, v128 (lo), i64,
    // v128 (lo again -- the *last* return value, so its overflow spills
    // past the whole return area instead of into another return slot).
    ...wasmF64Const(1.0),
    ...wasmF64Const(2.0),
    kExprLocalGet, 1,
    ...wasmI64Const(42),
    kExprLocalGet, 1,
  ]).exportFunc();

// Wrapper that calls `inner`, keeping `extra` live across the call, and
// extracts it as i32 lanes (since v128 values cannot cross the JS/wasm
// boundary). `inner`'s own return values are dropped -- only `extra`'s
// spill slot is relevant to this test.
builder.addFunction('test', makeSig(
    [kWasmI32],
    [kWasmI32, kWasmI32, kWasmI32, kWasmI32]))
  .addLocals(kWasmS128, 1)  // 1: extra
  .addBody([
    // extra = i32x4.add(v128.load(offset + 32), splat(1)). Computed before
    // the call and used after it, so it must stay live across the call to
    // `inner` and gets spilled somewhere in `test`'s own frame -- possibly
    // adjacent to `inner`'s outgoing return-value area.
    kExprLocalGet, 0,
    kSimdPrefix, ...wasmUnsignedLeb(kExprS128LoadMem), 0, 32,
    kExprI32Const, 1,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Splat),
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4Add),
    kExprLocalSet, 1,

    kExprLocalGet, 0,
    kExprCallFunction, inner.index,
    kExprDrop,  // v128 (lo again)
    kExprDrop,  // i64
    kExprDrop,  // v128 (lo)
    kExprDrop,  // f64
    kExprDrop,  // f64

    // extra, checked *after* the call, to detect whether the overflow
    // (spilling past the return area) clobbered its spill slot.
    kExprLocalGet, 1,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4ExtractLane), 0,
    kExprLocalGet, 1,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4ExtractLane), 1,
    kExprLocalGet, 1,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4ExtractLane), 2,
    kExprLocalGet, 1,
    kSimdPrefix, ...wasmUnsignedLeb(kExprI32x4ExtractLane), 3,
  ]).exportFunc();

const instance = builder.instantiate();
const mem = new Int32Array(instance.exports.memory.buffer);
for (let i = 0; i < 12; i++) {
  mem[i] = i + 100;
}

// Run once on Liftoff first.
instance.exports.test(0);

// `inner` is only reachable via a direct call from `test`, so it must be
// tiered up explicitly for revectorization to run on it (and must be
// exported so %WasmTierUpFunction can reference it, even though it's never
// called directly from JS since it returns v128 values).
%WasmTierUpFunction(instance.exports.inner);
%WasmTierUpFunction(instance.exports.test);

// This should not crash (the bug manifested as a wasm type assertion
// violation when the overflow clobbered unrelated stack content), and
// `extra` -- live across the call to `inner` -- must not be clobbered by the
// overflowing 32-byte write of `inner`'s last return value.
const result = instance.exports.test(0);

// extra = memory[8..11] + 1.
assertEquals([109, 110, 111, 112], result);
