// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kThreshold = 0x880000;
const kBranchMask = 0x00080000;
const builder = new WasmModuleBuilder();
const boxType = builder.addStruct([makeField(kWasmI31Ref, true)]);
const importSig = builder.addType(makeSig([], []));
const probeSig =
    builder.addType(makeSig([wasmRefNullType(boxType), kWasmI32], [kWasmI32]));

const collectImport = builder.addImport('m', 'collect', importSig);
const oldIndexGlobal =
    builder.addGlobal(kWasmI32, true, false).exportAs('oldIndex');
const takenGlobal = builder.addGlobal(kWasmI32, true, false).exportAs('taken');

builder.addFunction('makeBox', makeSig([kWasmI32], [wasmRefType(boxType)]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefI31,
      kGCPrefix, kExprStructNew, boxType,
    ])
    .exportFunc();

// Subtest A: comparison on i31.get_s evaluated before a GC-triggering import
// call and branched on after the call must not sink the comparison on the
// GC-relocated tagged register across the call.
builder.addFunction('probeSunkComparison', probeSig)
    .addLocals(kWasmI32, 2)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, boxType, 0,
      kGCPrefix, kExprI31GetS,
      kExprLocalTee, 2,
      ...wasmI32Const(kThreshold),
      kExprI32LtU,
      kExprLocalSet, 3,
      kExprLocalGet, 2,
      kExprGlobalSet, oldIndexGlobal.index,
      kExprLocalGet, 1,
      kExprIf, kWasmVoid,
        kExprCallFunction, collectImport,
      kExprEnd,
      kExprLocalGet, 3,
      kExprIf, kWasmI32,
        ...wasmI32Const(111),
        kExprGlobalSet, takenGlobal.index,
        kExprLocalGet, 2,
      kExprElse,
        ...wasmI32Const(222),
        kExprGlobalSet, takenGlobal.index,
        kExprI32Const, 0x7f,
      kExprEnd,
    ])
    .exportFunc();

// Subtest B: i31.get_u evaluated before a GC-triggering import call and
// masked in a branch condition after the call must not fold (x >>> 1) & kMask
// into x & (kMask << 1) on the post-GC relocated tagged register.
builder.addFunction('probePostCallBranchMask', probeSig)
    .addLocals(kWasmI32, 1)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, boxType, 0,
      kGCPrefix, kExprI31GetU,
      kExprLocalTee, 2,
      kExprGlobalSet, oldIndexGlobal.index,
      kExprLocalGet, 1,
      kExprIf, kWasmVoid,
        kExprCallFunction, collectImport,
      kExprEnd,
      kExprLocalGet, 2,
      ...wasmI32Const(kBranchMask),
      kExprI32And,
      kExprIf, kWasmI32,
        ...wasmI32Const(111),
      kExprElse,
        ...wasmI32Const(222),
      kExprEnd,
    ])
    .exportFunc();

let doRelocate = false;
let target = null;

const instance = builder.instantiate({
  m: {
    collect() {
      if (!doRelocate) return;
      gc({type: 'minor'});
    },
  },
});

const wasm = instance.exports;
const box = wasm.makeBox(5);
const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const kBoxField0Offset = 8;

// Tier up the Wasm functions to Turboshaft.
%WasmTierUpFunction(wasm.probeSunkComparison);
%WasmTierUpFunction(wasm.probePostCallBranchMask);

// Allocate a movable young-space object in the upper semi-space
// (>= 2 * kThreshold, with kBranchMask << 1 set) so that minor GC evacuates
// it to the lower semi-space (< 2 * kThreshold, with kBranchMask << 1 clear).
function prepareYoungTarget() {
  target = {value: 42};
  if (((Sandbox.getAddressOf(target) >>> 1) & kBranchMask) === 0) {
    gc({type: 'minor'});
    target = {value: 42};
  }
  memory.setUint32(Sandbox.getAddressOf(box) + kBoxField0Offset,
                   Sandbox.getAddressOf(target) + 1, true);
}

// Verify Subtest A: probeSunkComparison must not take the in-bounds branch
// when the pre-GC index was out of bounds (>= kThreshold).
prepareYoungTarget();
doRelocate = true;
const sunkRes = wasm.probeSunkComparison(box, 1);
if ((wasm.oldIndex.value >>> 0) >= kThreshold) {
  assertEquals(222, wasm.taken.value);
  assertEquals(-1, sunkRes);
} else {
  assertEquals(111, wasm.taken.value);
  assertEquals(wasm.oldIndex.value, sunkRes);
}

// Verify Subtest B: probePostCallBranchMask must branch on the pre-GC untagged
// value rather than reading the post-GC relocated tagged register.
prepareYoungTarget();
doRelocate = true;
const branchRes = wasm.probePostCallBranchMask(box, 1);
const expectedBranch =
    (wasm.oldIndex.value & kBranchMask) !== 0 ? 111 : 222;
assertEquals(expectedBranch, branchRes);
