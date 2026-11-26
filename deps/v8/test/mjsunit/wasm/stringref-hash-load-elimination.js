// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-stringref --turboshaft-verify-load-elimination
// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const structType = builder.addStruct([makeField(kWasmI32, true)]);
const g = builder.addGlobal(wasmRefNullType(structType), true, false);

builder.addFunction('init', kSig_v_v)
    .addBody([
      kExprI32Const, 0,
      kGCPrefix, kExprStructNew, structType,
      kExprGlobalSet, g.index,
    ])
    .exportFunc();

// Two string.hash calls on the same unhashed string separated by a struct.set
// (at a different offset from Name::raw_hash_field_) so GVN at the bottom of
// WasmOptimizePhase increments its epoch and does not fold
// LateLoadElimination's verification load back to the first raw_hash_field_
// load.
builder.addFunction('hashTwice', makeSig([kWasmStringRef], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      ...GCInstr(kExprStringHash),
      kExprGlobalGet, g.index,
      kExprI32Const, 1,
      kGCPrefix, kExprStructSet, structType, 0,
      kExprLocalGet, 0,
      ...GCInstr(kExprStringHash),
      kExprI32Sub,
    ])
    .exportFunc();

const instance = builder.instantiate();
instance.exports.init();
%WasmTierUpFunction(instance.exports.hashTwice);

function makeUnhashed(n) {
  return 'unhashed_string_' + n;
}
assertEquals(0, instance.exports.hashTwice(makeUnhashed(1)));
