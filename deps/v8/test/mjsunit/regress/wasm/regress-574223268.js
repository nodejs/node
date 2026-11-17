// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-compaction

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function buildModule(numTargets, tableSize = numTargets) {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);
  const sig_v_v = builder.addType(kSig_v_v);
  const sig_v_i = builder.addType(kSig_v_i);
  const funcs = [];
  for (let i = 0; i < numTargets; i++) {
    const f = builder.addFunction('f' + i, sig_v_v)
        .addBody([
          kExprI32Const, 0,
          kExprI32Const, 42 + i,
          kExprI32StoreMem, 0, 0,
        ])
        .exportFunc();
    funcs.push(f.index);
  }
  builder.addFunction('call', sig_v_i)
      .addBody([
        kExprLocalGet, 0,
        kExprCallIndirect, sig_v_v, kTableZero,
      ])
      .exportFunc();
  builder.setTableBounds(tableSize, tableSize);
  builder.addActiveElementSegment(0, wasmI32Const(0), funcs);
  return builder.toModule();
}

// Fill the bootstrap trusted page(s) (which are marked never_evacuate) so that
// subsequent trusted objects land on movable pages.
const filler = [];
const fillerModule = buildModule(1, 2000);
for (let i = 0; i < 64; i++) {
  filler.push(new WebAssembly.Instance(fillerModule));
}

const targetModule = buildModule(2);
const instance = new WebAssembly.Instance(targetModule);

// Compile both callees up front.
instance.exports.f0();
instance.exports.f1();
// Make the call site monomorphic on target 0.
instance.exports.call(0);
// Force the monomorphic-miss FixedArray allocation inside CallIndirectIC to
// trigger a compacting GC while WasmTrustedInstanceData is on the stack.
%SimulateNewspaceFull();
instance.exports.call(1);
