// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-debug-code

// Regression test for b/581561827: JumpThreading must not forward a
// gap-jump block to the function entry block (B0) when B0 constructs a frame,
// as branching back to B0 re-executes AssembleConstructFrame() and corrupts
// the stack frame.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const id_mask = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];

const g = builder.addGlobal(kWasmI32, true, false);
const sig_index = builder.addType(makeSig([kWasmS128, kWasmS128], [kWasmI32]));

const leaf = builder.addFunction('leaf', sig_index)
  .addLocals(kWasmS128, 2)  // local 2 = x, local 3 = tmp
  .addBody([
    // Entry block B0: leaf elides kFunctionEntry stack check, so B0 contains
    // only a ParallelMove (vmovapd xmm0, xmm2) + kArchJmp to the loop header,
    // while FrameElider sets B0->must_construct_frame() due to kLoop check.
    kExprLocalGet, 1,
    kExprLocalGet, 0,
    ...SimdInstr(kExprI8x16Shuffle), ...id_mask,
    kExprLocalSet, 2,

    kExprBlock, kWasmI32,
      kExprLoop, kWasmVoid,
        kExprGlobalGet, g.index,
        kExprI32Const, 0,
        kExprI32LeS,
        kExprIf, kWasmVoid,
          kExprLocalGet, 2,
          ...SimdInstr(kExprI32x4ExtractLane), 0,
          kExprBr, 2,
        kExprEnd,

        // Ensure param 1 (b) interferes with loop phi x.
        kExprLocalGet, 2,
        kExprLocalGet, 1,
        ...SimdInstr(kExprI32x4Add),
        kExprLocalSet, 3,

        kExprGlobalGet, g.index,
        kExprI32Const, 1,
        kExprI32Sub,
        kExprGlobalSet, g.index,

        // Two backedges converging at the loop-merge block allow
        // MoveOptimizer::OptimizeMerge to bail out at the loop header and then
        // hoist the shared gap move into the merge block, leaving B0 and the
        // loop backedge with identical gap moves targeting the loop header.
        kExprGlobalGet, g.index,
        kExprI32Const, 1,
        kExprI32And,
        kExprIf, kWasmVoid,
          kExprLocalGet, 1,
          kExprLocalGet, 3,
          ...SimdInstr(kExprI8x16Shuffle), ...id_mask,
          kExprLocalSet, 2,
          kExprBr, 1,
        kExprElse,
          kExprLocalGet, 1,
          kExprLocalGet, 3,
          kExprLocalGet, 0,
          ...SimdInstr(kExprI32x4Add),
          ...SimdInstr(kExprI8x16Shuffle), ...id_mask,
          kExprLocalSet, 2,
          kExprBr, 1,
        kExprEnd,
        kExprUnreachable,
      kExprEnd,
      kExprI32Const, 0,
    kExprEnd,
  ])
  .exportFunc();

const table = builder.addTable(kWasmAnyFunc, 1, 1);
builder.addActiveElementSegment(table.index, wasmI32Const(0), [leaf.index]);

builder.addFunction('main', makeSig([kWasmI32, kWasmI32], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kExprGlobalSet, g.index,
    kExprI32Const, 7, ...SimdInstr(kExprI32x4Splat),
    kExprI32Const, 11, ...SimdInstr(kExprI32x4Splat),
    kExprLocalGet, 1,
    kExprCallIndirect, sig_index, table.index,
  ])
  .exportFunc();

const instance = builder.instantiate();
%WasmTierUpFunction(instance.exports.leaf);
assertEquals(11, instance.exports.main(100, 0));
