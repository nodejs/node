// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Pre-expand both young semi-spaces before testing so young semi-space
// compressed addresses straddle `2 * kThreshold` (`0x1040000` vs `0x1090000`).
gc({type: 'minor'});
gc({type: 'minor'});

const kThreshold = 0x838000;
const builder = new WasmModuleBuilder();
const boxType = builder.addStruct([makeField(kWasmI31Ref, true)]);
const voidSig = builder.addType(kSig_v_v);
const probeSig = builder.addType(makeSig([wasmRefType(boxType)], [kWasmI32]));

const collectImport = builder.addImport('m', 'collect', voidSig);
const sinkGlobal = builder.addGlobal(kWasmI32, true, false);

builder.addFunction('makeBox', makeSig([kWasmI32], [wasmRefType(boxType)]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefI31,
      kGCPrefix, kExprStructNew, boxType,
    ])
    .exportFunc();

// - Pre-call: `index = i31.get_s(struct.get(box, 0))` spills as untagged w32,
//   and `((index << 1) ^ 4)` consumes `TruncateWord64ToWord32(bitcast)`
//   directly.
// - Post-call: `index <u kThreshold` must not look through
//   `TruncateWord64ToWord32` to the GC-relocated `kCompressed` spill slot or
//   bypass the bitcast in `VisitTruncateInt64ToInt32`.
builder.addFunction('probe', probeSig)
    .addLocals(kWasmI32, 1)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, boxType, 0,
      kGCPrefix, kExprI31GetS,
      kExprLocalTee, 1,
      kExprI32Const, 1,
      kExprI32Shl,
      kExprI32Const, 4,
      kExprI32Xor,
      kExprGlobalSet, sinkGlobal.index,
      kExprCallFunction, collectImport,
      kExprLocalGet, 1,
      ...wasmI32Const(kThreshold),
      kExprI32LtU,
      kExprIf, kWasmI32,
        kExprI32Const, 1,
      kExprElse,
        kExprI32Const, 0,
      kExprEnd,
    ])
    .exportFunc();

let doRelocate = false;
const instance = builder.instantiate({
  m: {
    collect() {
      if (doRelocate) gc({type: 'minor'});
    },
  },
});

const wasm = instance.exports;
const box = wasm.makeBox(0);
%WasmTierUpFunction(wasm.probe);

function makeYoungObject() {
  return [1, 2, 3];
}

// Ensure `target` starts in the upper young semi-space (>= 2 * kThreshold) so
// minor GC evacuates it downward into the lower semi-space (< 2 * kThreshold).
let target = makeYoungObject();
if ((Sandbox.getAddressOf(target) >>> 1) < kThreshold) {
  gc({type: 'minor'});
  target = makeYoungObject();
}

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const kBoxField0Offset = 8;
if ((Sandbox.getAddressOf(target) >>> 1) >= kThreshold) {
  memory.setUint32(
      Sandbox.getAddressOf(box) + kBoxField0Offset,
      Sandbox.getAddressOf(target) + 1, true);
  doRelocate = true;
  assertEquals(0, wasm.probe(box));
}
