// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-turbo-graph --no-liftoff --no-wasm-lazy-compilation
// Flags: --no-wasm-loop-peeling --no-wasm-loop-unrolling
// Flags: --no-verify-turboshaft --wasm-custom-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.startRecGroup();
const Described = builder.addStruct({fields: [], descriptor: 1});
const DescBox = builder.addStruct({
  fields: [makeField(kWasmI32, false)],
  describes: Described,
});
builder.endRecGroup();

// The do-while loop body always executes at least once, so the immutable load
// of `param1.field0` inside the loop dominates the loop exit (replayed via
// `SnapshotTable::OnValueChange`). Allocating a descriptor struct after the
// loop goes through a builtin call (`InvalidateMaybeAliasing`) rather than
// `WasmAllocateStruct`, so its initializing `StructSet` walks `offset_keys_` in
// `Invalidate` for a non-fresh `DescBox` base. Because immutable keys stay out
// of `base_keys_` and `offset_keys_`, `param1.field0` is preserved and the
// second `struct.get` is eliminated.
//
// CHECK-LABEL: Begin compiling method immutableFieldAcrossLoopExit using Turboshaft
// CHECK-LABEL: V8.TFTurboshaftWasmGCOptimize
// CHECK: StructGet(
// CHECK-NOT: StructGet(
// CHECK: Return(
builder
    .addFunction(
        'immutableFieldAcrossLoopExit',
        makeSig([kWasmI32, wasmRefType(DescBox)], [kWasmI32]))
    .addBody([
      kExprLoop, kWasmVoid,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructGet, DescBox, 0,
        kExprLocalGet, 0,
        kExprI32Add,
        kExprBrIf, 0,
      kExprEnd,
      kExprI32Const, 99,
      kGCPrefix, kExprStructNew, DescBox,
      kExprDrop,
      kExprLocalGet, 1,
      kGCPrefix, kExprStructGet, DescBox, 0,
    ]);

builder.toModule();
