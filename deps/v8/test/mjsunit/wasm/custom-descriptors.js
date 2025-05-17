// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(() => {
  let bad_builder = new WasmModuleBuilder();
  let $desc0 = bad_builder.nextTypeIndex() + 1;
  let $s0 = bad_builder.addStruct({fields: [], descriptor: $desc0});
  bad_builder.addStruct({fields: [], describes: $s0});
  // The recgroup is missing.
  assertThrows(() => bad_builder.instantiate(), WebAssembly.CompileError,
               /descriptor type index 1 is out of bounds/);
})();

let builder = new WasmModuleBuilder();

let i32_field = makeField(kWasmI32, true);
let i64_field = makeField(kWasmI64, true);
let sig_r_i = makeSig([kWasmI32], [kWasmAnyRef]);
let sig_r_r = makeSig([kWasmAnyRef], [kWasmAnyRef]);
let sig_v_r = makeSig([kWasmAnyRef], []);
let sig_i_r = makeSig([kWasmAnyRef], [kWasmI32])
let anyref = wasmRefNullType(kWasmAnyRef);

builder.startRecGroup();
const kNumStructs = 2;
let $desc0 = builder.nextTypeIndex() + kNumStructs;
let $desc1 = $desc0 + 1;
let $s0 = builder.addStruct({fields: [i32_field], descriptor: $desc0});
let $s1 = builder.addStruct(
  {fields: [i32_field, i32_field], descriptor: $desc1, supertype: $s0});

/* $desc0 */ builder.addStruct({fields: [i64_field], describes: $s0});
/* $desc1 */ builder.addStruct(
    {fields: [i64_field, i64_field], describes: $s1, supertype: $desc0});
builder.endRecGroup();

let $g_desc0 = builder.addGlobal(wasmRefType($desc0).exact(), false, false,
                                [kGCPrefix, kExprStructNewDefault, $desc0]);

let $g_desc1 = builder.addGlobal(wasmRefType($desc1).exact(), false, false,
                                 [kGCPrefix, kExprStructNewDefault, $desc1]);

// For convenience, we ensure matching indices:
assertEquals($g_desc0.index, $s0);
assertEquals($g_desc1.index, $s1);

function MakeFunctions(type_index) {
  let desc_index = type_index + kNumStructs;  // By type section construction.
  let global_index = type_index;

  builder.addFunction("make_s" + type_index, sig_r_i).exportFunc().addBody([
    kExprLocalGet, 0,
    ...(type_index == $s1 ? [kExprI32Const, 2] : []),
    kExprGlobalGet, global_index,
    kGCPrefix, kExprStructNew, type_index,
  ]);

  builder.addFunction("oneshot_s" + type_index, sig_r_i).exportFunc().addBody([
    kExprLocalGet, 0,
    ...(type_index == $s1 ? [kExprI32Const, 2] : []),
    kGCPrefix, kExprStructNewDefault, desc_index,
    kGCPrefix, kExprStructNew, type_index,
  ]);

  builder.addFunction("get_desc_s" + type_index, sig_r_r).exportFunc().addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCast, type_index,
    kGCPrefix, kExprRefGetDesc, type_index,
  ]);

  builder.addFunction("cast_desc_s" + type_index, sig_v_r).exportFunc()
  .addBody([
    kExprLocalGet, 0,
    kExprGlobalGet, global_index,
    kGCPrefix, kExprRefCastDesc, type_index,
    kExprDrop,
    // Same thing again, with an exact cast.
    kExprLocalGet, 0,
    kExprGlobalGet, global_index,
    kGCPrefix, kExprRefCastDesc, kWasmExact, type_index,
    kExprDrop,
  ]);

  builder.addFunction("cast_desc_null_s" + type_index, sig_v_r).exportFunc()
  .addBody([
    kExprLocalGet, 0,
    kExprGlobalGet, global_index,
    kGCPrefix, kExprRefCastDescNull, type_index,
    kExprDrop,
    // Same thing again, with an exact cast.
    kExprLocalGet, 0,
    kExprGlobalGet, global_index,
    kGCPrefix, kExprRefCastDescNull, kWasmExact, type_index,
    kExprDrop,
  ]);

  // Increments the descriptor's first field by 1, and returns the
  // incremented value.
  builder.addFunction("inc_desc_field_s" + type_index, sig_i_r).exportFunc()
  .addLocals(wasmRefType(desc_index), 1)
  .addLocals(kWasmI64, 1)
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCast, type_index,
    kGCPrefix, kExprRefGetDesc, type_index,
    kExprLocalTee, 1,  // One copy for struct.set.
    kExprLocalGet, 1,  // One copy for struct.get.
    kGCPrefix, kExprStructGet, desc_index, 0,
    kExprI64Const, 1,
    kExprI64Add,
    kExprLocalTee, 2,
    kGCPrefix, kExprStructSet, desc_index, 0,
    kExprLocalGet, 2,
    kExprI32ConvertI64,
  ]);

  builder.addFunction("br_on_cast_desc_s" + type_index, sig_i_r)
  .exportFunc()
  .addBody([
    kExprBlock, kAnyRefCode,
      kExprLocalGet, 0,
      kExprGlobalGet, global_index,
      ...wasmBrOnCastDesc(0, anyref, wasmRefNullType(type_index).exact()),
      kExprI32Const, 0, // If branch not taken, return 0.
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1, // If branch taken, return 1.
  ]);

builder.addFunction("br_on_cast_desc_fail_s" + type_index, sig_i_r)
  .exportFunc()
  .addBody([
    kExprBlock, kAnyRefCode,
      kExprLocalGet, 0,
      kExprGlobalGet, global_index,
      ...wasmBrOnCastDescFail(0, anyref, wasmRefType(type_index)),
      kExprI32Const, 0, // If branch not taken (cast succeeded), return 0.
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1, // If branch taken (cast failed), return 1.
  ]);
}

MakeFunctions($s0);
MakeFunctions($s1);

let instance = builder.instantiate();
let wasm = instance.exports;

let s0 = wasm.make_s0(11);
let s1 = wasm.make_s1(22);

let oneshot_s0 = wasm.oneshot_s0(33);
let oneshot_s0a = wasm.oneshot_s0(34);
let oneshot_s1 = wasm.oneshot_s1(44);

assertSame(wasm.get_desc_s0(s0), wasm.get_desc_s0(s0));
// Treating s1 as its supertype s0 works for get_desc:
assertSame(wasm.get_desc_s0(s1), wasm.get_desc_s1(s1));
assertNotSame(wasm.get_desc_s0(s0), wasm.get_desc_s0(oneshot_s0));
assertNotSame(wasm.get_desc_s0(oneshot_s0), wasm.get_desc_s0(oneshot_s0a));
assertNotSame(wasm.get_desc_s1(s1), wasm.get_desc_s1(oneshot_s1));

wasm.cast_desc_s0(s0);
wasm.cast_desc_s1(s1);
assertThrows(
    () => wasm.cast_desc_s1(s0), WebAssembly.RuntimeError, 'illegal cast');
assertThrows(
    () => wasm.cast_desc_s0(s1), WebAssembly.RuntimeError, 'illegal cast');

wasm.cast_desc_null_s0(s0);
wasm.cast_desc_null_s1(s1);
wasm.cast_desc_null_s0(null);
wasm.cast_desc_null_s1(null);
assertThrows(
    () => wasm.cast_desc_null_s1(s0), WebAssembly.RuntimeError, 'illegal cast');
assertThrows(
    () => wasm.cast_desc_null_s0(s1), WebAssembly.RuntimeError, 'illegal cast');

let s0_other = wasm.make_s0(33);
assertEquals(1, wasm.inc_desc_field_s0(s0));
assertEquals(2, wasm.inc_desc_field_s0(s0_other));
assertEquals(3, wasm.inc_desc_field_s0(s0));

let s1_other = wasm.make_s1(44);
assertEquals(1, wasm.inc_desc_field_s1(s1));
assertEquals(2, wasm.inc_desc_field_s1(s1_other));
assertEquals(3, wasm.inc_desc_field_s1(s1));
assertEquals(4, wasm.inc_desc_field_s1(s1_other));
assertEquals(5, wasm.inc_desc_field_s1(s1));

// inc_desc_field_s0, when operating on s1-typed structs, still updates
// the $desc1 descriptor.
assertEquals(6, wasm.inc_desc_field_s0(s1_other));
assertEquals(7, wasm.inc_desc_field_s0(s1));

assertEquals(1, wasm.br_on_cast_desc_s0(s0));
assertEquals(0, wasm.br_on_cast_desc_s0(s1));
assertEquals(1, wasm.br_on_cast_desc_s0(null));

assertEquals(0, wasm.br_on_cast_desc_s1(s0));
assertEquals(1, wasm.br_on_cast_desc_s1(s1));
assertEquals(1, wasm.br_on_cast_desc_s1(null));

assertEquals(0, wasm.br_on_cast_desc_fail_s0(s0));
assertEquals(1, wasm.br_on_cast_desc_fail_s0(s1));
assertEquals(1, wasm.br_on_cast_desc_fail_s0(null));

assertEquals(1, wasm.br_on_cast_desc_fail_s1(s0));
assertEquals(0, wasm.br_on_cast_desc_fail_s1(s1));
assertEquals(1, wasm.br_on_cast_desc_fail_s1(null));
