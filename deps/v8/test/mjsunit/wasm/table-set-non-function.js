// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wasmfx --expose-gc --verify-heap --no-liftoff

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestInlineNonFunctionTableSet() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const struct_type = builder.addStruct([makeField(kWasmI32, false)]);
  const struct_ref = wasmRefNullType(struct_type);
  const array_type = builder.addArray(kWasmI32, {mutable: true});
  const array_ref = wasmRefNullType(array_type);
  const sig_i_v = builder.addType(kSig_i_v);
  const cont_i_v = builder.addCont(sig_i_v);
  const cont_ref = wasmRefNullType(cont_i_v);

  const extern_table = builder.addTable(kWasmExternRef, 4).exportAs("extern_t");
  const any_table = builder.addTable(kWasmAnyRef, 4).exportAs("any_t");
  const struct_table = builder.addTable(struct_ref, 4).exportAs("struct_t");
  const array_table = builder.addTable(array_ref, 4).exportAs("array_t");
  const cont_table = builder.addTable(cont_ref, 4).exportAs("cont_t");
  const empty_table = builder.addTable(kWasmExternRef, 0).exportAs("empty_t");
  const extern_table64 =
      builder.addTable64(kWasmExternRef, 4).exportAs("extern_t64");

  const fn_ret_77 =
      builder.addFunction("ret77", sig_i_v).addBody([...wasmI32Const(77)]);
  builder.addDeclarativeElementSegment([fn_ret_77.index]);

  builder.addFunction("set_extern", makeSig([kWasmI32, kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprTableSet, extern_table.index,
      ]).exportFunc();

  builder.addFunction("set_extern_null", kSig_v_i)
      .addBody([
        kExprLocalGet, 0,
        kExprRefNull, kExternRefCode,
        kExprTableSet, extern_table.index,
      ]).exportFunc();

  builder.addFunction("get_extern", makeSig([kWasmI32], [kWasmExternRef]))
      .addBody([
        kExprLocalGet, 0,
        kExprTableGet, extern_table.index,
      ]).exportFunc();

  builder.addFunction("set_extern64", makeSig([kWasmI64, kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprTableSet, extern_table64.index,
      ]).exportFunc();

  builder.addFunction("get_extern64", makeSig([kWasmI64], [kWasmExternRef]))
      .addBody([
        kExprLocalGet, 0,
        kExprTableGet, extern_table64.index,
      ]).exportFunc();

  builder.addFunction("set_new_struct", kSig_v_ii)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructNew, struct_type,
        kExprTableSet, struct_table.index,
      ]).exportFunc();

  builder.addFunction("set_struct_null", kSig_v_i)
      .addBody([
        kExprLocalGet, 0,
        kExprRefNull, struct_type,
        kExprTableSet, struct_table.index,
      ]).exportFunc();

  builder.addFunction("get_struct_val", kSig_i_i)
      .addBody([
        kExprLocalGet, 0,
        kExprTableGet, struct_table.index,
        kGCPrefix, kExprStructGet, struct_type, 0,
      ]).exportFunc();

  builder.addFunction("is_struct_null", kSig_i_i)
      .addBody([
        kExprLocalGet, 0,
        kExprTableGet, struct_table.index,
        kExprRefIsNull,
      ]).exportFunc();

  builder.addFunction("store_new_cont", kSig_v_i)
      .addBody([
        kExprLocalGet, 0,
        kExprRefFunc, fn_ret_77.index,
        kExprContNew, cont_i_v,
        kExprTableSet, cont_table.index,
      ]).exportFunc();

  builder.addFunction("set_cont_null", kSig_v_i)
      .addBody([
        kExprLocalGet, 0,
        kExprRefNull, cont_i_v,
        kExprTableSet, cont_table.index,
      ]).exportFunc();

  builder.addFunction("resume_cont_from_table", kSig_i_i)
      .addBody([
        kExprLocalGet, 0,
        kExprTableGet, cont_table.index,
        kExprResume, cont_i_v, 0,
      ]).exportFunc();

  builder.addFunction("is_cont_null", kSig_i_i)
      .addBody([
        kExprLocalGet, 0,
        kExprTableGet, cont_table.index,
        kExprRefIsNull,
      ]).exportFunc();

  builder.addFunction("set_empty", makeSig([kWasmI32, kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprTableSet, empty_table.index,
      ]).exportFunc();

  builder.addFunction("grow_extern", makeSig([kWasmI32], [kWasmI32]))
      .addBody([
        kExprRefNull, kExternRefCode,
        kExprLocalGet, 0,
        kNumericPrefix, kExprTableGrow, extern_table.index,
      ]).exportFunc();

  // Exercises same-function table.set -> table.get, arrayref (kArray), i31ref
  // in anyref (kOther Smi), and table.set -> table.grow -> table.set/get load
  // elimination invalidation.
  builder.addFunction("set_grow_set_extern",
                      makeSig([kWasmExternRef, kWasmExternRef],
                              [kWasmExternRef]))
      .addBody([
        kExprI32Const, 2,
        kExprLocalGet, 0,
        kExprTableSet, extern_table.index,
        kExprRefNull, kExternRefCode,
        kExprI32Const, 2,
        kNumericPrefix, kExprTableGrow, extern_table.index,
        kExprDrop,
        // Store into newly grown index 7 and read back from index 2 and 7.
        kExprI32Const, 7,
        kExprLocalGet, 1,
        kExprTableSet, extern_table.index,
        kExprI32Const, 2,
        kExprTableGet, extern_table.index,
        kExprDrop,
        kExprI32Const, 7,
        kExprTableGet, extern_table.index,
      ]).exportFunc();

  builder.addFunction("set_get_array_and_i31", kSig_i_i)
      .addBody([
        kExprI32Const, 0,
        kExprLocalGet, 0,
        kExprI32Const, 2,
        kGCPrefix, kExprArrayNew, array_type,
        kExprTableSet, array_table.index,

        kExprI32Const, 1,
        kExprLocalGet, 0,
        kGCPrefix, kExprRefI31,
        kExprTableSet, any_table.index,

        kExprI32Const, 0,
        kExprTableGet, array_table.index,
        kExprI32Const, 1,
        kGCPrefix, kExprArrayGet, array_type,
        kExprI32Const, 1,
        kExprTableGet, any_table.index,
        kGCPrefix, kExprRefCast, kI31RefCode,
        kGCPrefix, kExprI31GetS,
        kExprI32Add,
      ]).exportFunc();

  const instance = builder.instantiate();

  // Promote the tables and their FixedArrays into old space.
  gc();
  gc();

  // Store young objects into old-space tables (exercises Scavenge barrier).
  const young_obj = {k: "val"};
  instance.exports.set_extern(0, young_obj);
  instance.exports.set_extern(1, 123);
  instance.exports.set_extern(3, young_obj);
  instance.exports.set_extern64(2n, young_obj);
  instance.exports.set_new_struct(0, 42);
  instance.exports.store_new_cont(0);
  assertEquals(18, instance.exports.set_get_array_and_i31(9));

  gc({type: "minor"});
  assertSame(young_obj, instance.exports.get_extern(0));
  assertEquals(123, instance.exports.get_extern(1));
  assertSame(young_obj, instance.exports.get_extern(3));
  assertSame(young_obj, instance.exports.get_extern64(2n));
  assertEquals(42, instance.exports.get_struct_val(0));
  assertEquals(77, instance.exports.resume_cont_from_table(0));

  // Clear entries with ref.null and verify across minor and major GC.
  instance.exports.set_extern_null(0);
  instance.exports.set_struct_null(0);
  instance.exports.store_new_cont(1);
  instance.exports.set_cont_null(1);
  gc({type: "minor"});
  gc();
  assertSame(null, instance.exports.get_extern(0));
  assertEquals(1, instance.exports.is_struct_null(0));
  assertEquals(1, instance.exports.is_cont_null(1));

  // Grow extern_table (4 -> 6, then 6 -> 8 inside set_grow_set_extern).
  assertEquals(4, instance.exports.grow_extern(2));
  const grown_obj = {g: "grown"};
  const grown_obj2 = {g: "grown2"};
  instance.exports.set_extern(5, grown_obj);
  assertSame(
      grown_obj2,
      instance.exports.set_grow_set_extern(grown_obj, grown_obj2));
  gc({type: "minor"});
  assertSame(grown_obj, instance.exports.get_extern(5));
  assertSame(grown_obj, instance.exports.get_extern(2));
  assertSame(grown_obj2, instance.exports.get_extern(7));

  // Out-of-bounds traps.
  assertTraps(kTrapTableOutOfBounds, () => instance.exports.set_extern(-1, {}));
  assertTraps(kTrapTableOutOfBounds, () => instance.exports.set_extern(8, {}));
  assertTraps(kTrapTableOutOfBounds, () => instance.exports.set_empty(0, {}));
  assertTraps(
      kTrapTableOutOfBounds, () => instance.exports.set_extern64(-1n, {}));
  assertTraps(
      kTrapTableOutOfBounds, () => instance.exports.set_extern64(4n, {}));
  assertTraps(
      kTrapTableOutOfBounds,
      () => instance.exports.set_extern64(0x100000000n, {}));
  assertTraps(
      kTrapTableOutOfBounds, () => instance.exports.set_new_struct(4, 1));
  assertTraps(
      kTrapTableOutOfBounds, () => instance.exports.store_new_cont(4));
})();
