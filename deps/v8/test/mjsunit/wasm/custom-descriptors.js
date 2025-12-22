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
let sig_i_r = makeSig([kWasmAnyRef], [kWasmI32]);
let sig_i_v = makeSig([], [kWasmI32]);
let sig_v_v = makeSig([], []);
let anyref = wasmRefNullType(kWasmAnyRef);
let funcref = wasmRefNullType(kWasmFuncRef);
let nullref = wasmRefNullType(kWasmNullRef);

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

let $g_s0 = builder.addGlobal(wasmRefType($s0), false, false, [
  kExprGlobalGet, $g_desc0.index,
  kGCPrefix, kExprStructNewDefaultDesc, $s0,
]);

let $sideeffect = builder.addImport("m", "sideeffect", kSig_v_v);

function MakeFunctions(type_index) {
  let desc_index = type_index + kNumStructs;  // By type section construction.
  let global_index = type_index;

  builder.addFunction("make_s" + type_index, sig_r_i).exportFunc().addBody([
    kExprLocalGet, 0,
    ...(type_index == $s1 ? [kExprI32Const, 2] : []),
    kExprGlobalGet, global_index,
    kGCPrefix, kExprStructNewDesc, type_index,
  ]);

  builder.addFunction("oneshot_s" + type_index, sig_r_i).exportFunc().addBody([
    kExprLocalGet, 0,
    ...(type_index == $s1 ? [kExprI32Const, 2] : []),
    kGCPrefix, kExprStructNewDefault, desc_index,
    kGCPrefix, kExprStructNewDesc, type_index,
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

  // Custom descriptors relaxes the requirement that the cast target be a
  // subtype of the cast source. We can cast up from nullref.
  builder.addFunction("br_on_cast_desc_from_null_s" + type_index, sig_i_v)
    .exportFunc()
    .addBody([
      kExprBlock, kAnyRefCode,
        kExprRefNull, kNullRefCode,
        kExprGlobalGet, global_index,
        ...wasmBrOnCastDesc(0, nullref, wasmRefNullType(type_index)),
        kExprI32Const, 0, // If branch not taken, return 0.
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1, // If branch taken, return 1.
    ]);

  builder.addFunction("br_on_cast_desc_fail_from_null_s" + type_index, sig_i_v)
    .exportFunc()
    .addBody([
      kExprBlock, kAnyRefCode,
        kExprRefNull, kNullRefCode,
        kExprGlobalGet, global_index,
        ...wasmBrOnCastDescFail(0, nullref, wasmRefNullType(type_index)),
        kExprI32Const, 0, // If branch not taken, return 0.
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1, // If branch taken, return 1.
    ]);

  // The relaxation applies to normal br_on_cast as well.
  builder.addFunction("br_on_cast_from_null_s" + type_index, sig_i_v)
    .exportFunc()
    .addBody([
      kExprBlock, kAnyRefCode,
        kExprRefNull, kNullRefCode,
        ...wasmBrOnCast(0, nullref, wasmRefNullType(type_index)),
        kExprI32Const, 0, // If branch not taken, return 0.
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1, // If branch taken, return 1.
    ]);

  builder.addFunction("br_on_cast_fail_from_null_s" + type_index, sig_i_v)
    .exportFunc()
    .addBody([
      kExprBlock, kAnyRefCode,
        kExprRefNull, kNullRefCode,
        ...wasmBrOnCastFail(0, nullref, wasmRefNullType(type_index)),
        kExprI32Const, 0, // If branch not taken, return 0.
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1, // If branch taken, return 1.
    ]);
}

MakeFunctions($s0);
MakeFunctions($s1);

// Put a `ref.get_desc` instruction into a loop to exercise compiler
// optimizations more than simple functions do.
builder.addFunction("get_desc_in_loop", kSig_i_ii).exportFunc()
  .addLocals(kWasmI32, 1)
  .addBody([
    kExprLoop, kWasmVoid,
      // desc := var_cond ? get_desc(global.get $struct) : global.get $desc;
      kExprLocalGet, 1,
      kExprIf, kAnyRefCode,
        kExprGlobalGet, $g_s0.index,
        kGCPrefix, kExprRefGetDesc, $s0,
      kExprElse,
        kExprGlobalGet, $g_desc0.index,
      kExprEnd,

      // var_sum += struct.get(cast(desc))
      kGCPrefix, kExprRefCast, $desc0,
      kGCPrefix, kExprStructGet, $desc0, 0,
      kExprI32ConvertI64,
      kExprLocalGet, 2,  // sum
      kExprI32Add,
      kExprLocalSet, 2,

      // LateLoadElimination needs to forget everything here; type-aware
      // WasmLoadElimination can keep the descriptor cached.
      kExprCallFunction, $sideeffect,

      // while (var_loop > 0)
      kExprLocalGet, 0,
      kExprI32Const, 1,
      kExprI32Sub,
      kExprLocalTee, 0,
      kExprBrIf, 0,
    kExprEnd,

    // return var_sum
    kExprLocalGet, 2,
]);

// Doesn't actually have side effects, but the Wasm compiler doesn't know that.
function sideeffect() {}

let instance = builder.instantiate({m: {sideeffect}});
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
let final_desc0_field = 3;
assertEquals(final_desc0_field, wasm.inc_desc_field_s0(s0));

let loop_count = 20;
assertEquals(final_desc0_field * loop_count,
             wasm.get_desc_in_loop(loop_count, 1));


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

assertEquals(1, wasm.br_on_cast_desc_from_null_s0());
assertEquals(1, wasm.br_on_cast_desc_from_null_s1());
assertEquals(0, wasm.br_on_cast_desc_fail_from_null_s0());
assertEquals(0, wasm.br_on_cast_desc_fail_from_null_s1());
assertEquals(1, wasm.br_on_cast_from_null_s0());
assertEquals(1, wasm.br_on_cast_from_null_s1());
assertEquals(0, wasm.br_on_cast_fail_from_null_s0());
assertEquals(0, wasm.br_on_cast_fail_from_null_s1());

// All br_on_cast variants trying to cast across hierarchies should still be
// invalid.
(() => {
  for (let fail of [false, true]) {
    for (let desc of [false, true]) {
      let builder = new WasmModuleBuilder();
      builder.startRecGroup();
      let $desc0 = builder.nextTypeIndex() + 1;
      let $s0 = builder.addStruct({fields: [], descriptor: $desc0});
      builder.addStruct({fields: [], describes: $s0});
      builder.endRecGroup();

      let name = "br_on_cast" + (desc ? "_desc" : "") + (fail ? "_fail" : "");
      let cast = fail ? (desc ? wasmBrOnCastDescFail : wasmBrOnCastFail) :
                        (desc ? wasmBrOnCastDesc : wasmBrOnCast);

      builder.addFunction(name, sig_v_v).addBody([
        kExprBlock, kAnyRefCode,
          kExprUnreachable,
          ...cast(0, funcref, wasmRefNullType($s0)),
          kExprUnreachable,
        kExprEnd,
        kExprDrop
      ]);

      let expected = new RegExp(
          `.*\"${name}\" failed: invalid types for ${name}: source type ` +
          'funcref and target type \\(ref null 0\\) must be in the same ' +
          'reference type hierarchy.*');
      let buffer = builder.toBuffer();
      assertThrowsAsync(
          WebAssembly.compile(buffer), WebAssembly.CompileError, expected);
    }
  }
})();

// Using struct.new_desc to allocate a type without a descriptor is invalid.
(() => {
  for (let [expr, name] of
    [[kExprStructNewDesc, "struct.new_desc"],
      [kExprStructNewDefaultDesc, "struct.new_default_desc"]]) {
    let builder = new WasmModuleBuilder();
    let $nodesc = builder.addStruct({});
    builder.addFunction(`invalid-${name}`, sig_v_v).addBody([
      kGCPrefix, expr, $nodesc,
      kExprDrop
    ]);

    let expected = new RegExp(
      `.*"invalid-${name}" failed: descriptor allocation used for type 0 ` +
      'without descriptor.*');
    let buffer = builder.toBuffer();
    assertThrowsAsync(
      WebAssembly.compile(buffer), WebAssembly.CompileError, expected);
  }
})();

// TODO(tlively): Uncomment this once users have transitioned to the new
// struct.new_desc and struct.new_default_desc instructions.
// // Using struct.new to allocate a type with a descriptor is invalid.
// (() => {
//   for (let [expr, name] of
//     [[kExprStructNew, "struct.new"],
//       [kExprStructNewDefault, "struct.new_default"]]) {
//     let builder = new WasmModuleBuilder();
//     builder.startRecGroup();
//     let $desc = builder.nextTypeIndex() + 1;
//     let $withdesc = builder.addStruct({ descriptor: $desc });
//     builder.addStruct({ describes: $withdesc });
//     builder.endRecGroup();

//     builder.addFunction(`invalid-${name}`, sig_v_v).addBody([
//       kGCPrefix, kExprStructNewDefault, $desc,
//       kGCPrefix, expr, $withdesc,
//       kExprDrop
//     ]);

//     let expected = new RegExp(
//       `.*"invalid-${name}" failed: non-descriptor allocation used for type 0 ` +
//       'with descriptor.*');
//     let buffer = builder.toBuffer();
//     assertThrowsAsync(
//       WebAssembly.compile(buffer), WebAssembly.CompileError, expected);
//   }
// })();
