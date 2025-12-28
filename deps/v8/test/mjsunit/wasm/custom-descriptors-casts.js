// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $desc0 = builder.nextTypeIndex() + 1;
let $s0 = builder.addStruct({fields: [], descriptor: $desc0});
builder.addStruct({fields: [], describes: $s0});
let $desc1 = builder.nextTypeIndex() + 1;
let $s1 = builder.addStruct({fields: [], descriptor: $desc1, final: true});
builder.addStruct({fields: [], describes: $s1, final: true});
let $desc2 = builder.nextTypeIndex() + 1;
let $s2 = builder.addStruct({fields: [], supertype: $s0, descriptor: $desc2});
builder.addStruct({fields: [], supertype: $desc0, describes: $s2});
builder.endRecGroup();

let desc0_type = wasmRefType($desc0).exact();
let desc1_type = wasmRefType($desc1).exact();
let desc2_type = wasmRefType($desc2).exact();

let $glob_desc0 = builder.addGlobal(desc0_type, false, false, [
  kGCPrefix, kExprStructNew, $desc0,
]);
let $glob_desc0_other = builder.addGlobal(desc0_type, false, false, [
  kGCPrefix, kExprStructNew, $desc0,
]);

let $glob_desc1 = builder.addGlobal(desc1_type, false, false, [
  kGCPrefix, kExprStructNew, $desc1,
]);
let $glob_desc2 = builder.addGlobal(desc2_type, false, false, [
  kGCPrefix, kExprStructNew, $desc2,
]);

let $glob_s0 = builder.addGlobal(kWasmAnyRef, true, false, [
  kExprGlobalGet, $glob_desc0.index,
  kGCPrefix, kExprStructNewDesc, $s0,
]);
let $glob_s1 = builder.addGlobal(kWasmAnyRef, true, false, [
  kExprGlobalGet, $glob_desc1.index,
  kGCPrefix, kExprStructNewDesc, $s1,
]);

function MakeFuncs(name, type_index, global_index, exact_bit) {
  let sig_r_v = makeSig([], [wasmRefType(type_index)]);
  let exact = exact_bit ? [kWasmExact] : [];
  builder.addFunction("cast" + name, sig_r_v).exportFunc().addBody([
    kExprGlobalGet, global_index,
    kGCPrefix, kExprRefCast, ...exact, type_index,
  ]);
  builder.addFunction("test" + name, kSig_i_v).exportFunc().addBody([
    kExprGlobalGet, global_index,
    kGCPrefix, kExprRefTest, ...exact, type_index,
  ]);
  builder.addFunction("br" + name, kSig_i_v).exportFunc().addBody([
    kExprBlock, kAnyRefCode,
    kExprGlobalGet, global_index,
    kGCPrefix, kExprBrOnCast, 0b11, 0, kAnyRefCode, ...exact, type_index,
    kExprI32Const, 0,  // Branch not taken.
    kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,  // Branch taken.
  ]);
  builder.addFunction("brfail" + name, kSig_i_v).exportFunc().addBody([
    kExprBlock, kAnyRefCode,
    kExprGlobalGet, global_index,
    kGCPrefix, kExprBrOnCastFail, 0b11, 0, kAnyRefCode, ...exact, type_index,
    kExprI32Const, 1,  // Branch not taken.
    kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 0,  // Branch taken.
  ]);
}

MakeFuncs("_exact", $s0, $glob_s0.index, true);
MakeFuncs("_final_exact", $s1, $glob_s1.index, true);
MakeFuncs("_final", $s1, $glob_s1.index, false);

function MakeBranchFunc(name, opcode, obj, desc, from, to) {
  builder.addFunction(name, kSig_i_v).exportFunc().addBody([
    kExprBlock, kAnyRefCode,
    kExprGlobalGet, obj,
    kGCPrefix, kExprRefCast, kWasmExact, from,
    kExprGlobalGet, desc,
    kGCPrefix, opcode, 0b11, 0, from, to,
    kExprI32Const, 0,  // Branch not taken.
    kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,  // Branch taken.
  ]);
}

MakeBranchFunc("br_always", kExprBrOnCastDesc,
               $glob_s0.index, $glob_desc0.index, $s0, $s0);
// This is named "always" because the cast statically always succeeds, and
// "not" because at runtime it fails due to a different descriptor.
MakeBranchFunc("br_always_not", kExprBrOnCastDesc,
               $glob_s0.index, $glob_desc0_other.index, $s0, $s0);
MakeBranchFunc("br_never", kExprBrOnCastDesc,
               $glob_s0.index, $glob_desc2.index, $s0, $s2);
MakeBranchFunc("brfail_always", kExprBrOnCastDescFail,
               $glob_s0.index, $glob_desc0.index, $s0, $s0);
MakeBranchFunc("brfail_always_not", kExprBrOnCastDescFail,
               $glob_s0.index, $glob_desc0_other.index, $s0, $s0);
MakeBranchFunc("brfail_never", kExprBrOnCastDescFail,
               $glob_s0.index, $glob_desc2.index, $s0, $s2);

let instance = builder.instantiate();

instance.exports.cast_exact();  // Should not trap.
instance.exports.cast_final();  // Should not trap.
instance.exports.cast_final_exact();  // Should not trap.
assertEquals(1, instance.exports.test_exact());
assertEquals(1, instance.exports.test_final());
assertEquals(1, instance.exports.test_final_exact());
assertEquals(1, instance.exports.br_exact());
assertEquals(1, instance.exports.br_final());
assertEquals(1, instance.exports.br_final_exact());
assertEquals(1, instance.exports.brfail_exact());
assertEquals(1, instance.exports.brfail_final());
assertEquals(1, instance.exports.brfail_final_exact());

assertEquals(1, instance.exports.br_always());
assertEquals(0, instance.exports.br_always_not());
assertEquals(0, instance.exports.br_never());
assertEquals(0, instance.exports.brfail_always());
assertEquals(1, instance.exports.brfail_always_not());
assertEquals(1, instance.exports.brfail_never());
