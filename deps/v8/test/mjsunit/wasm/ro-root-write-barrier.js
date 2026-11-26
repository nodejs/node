// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap --expose-gc --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestReadOnlyRootNullStoresSkipWriteBarrier() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const elem_type = builder.addStruct([makeField(kWasmI32, false)]);
  const struct_type = builder.addStruct([
    makeField(kWasmAnyRef, true),
    makeField(kWasmExternRef, true),
  ]);
  const array_type = builder.addArray(kWasmAnyRef, {mutable: true});
  const any_table = builder.addTable(kWasmAnyRef, 4).exportAs("any_table");
  const extern_table =
      builder.addTable(kWasmExternRef, 4).exportAs("extern_table");

  const g_struct = builder.addGlobal(wasmRefNullType(struct_type), true)
                       .exportAs("g_struct");
  const g_array =
      builder.addGlobal(wasmRefNullType(array_type), true).exportAs("g_array");
  const g_extern =
      builder.addGlobal(kWasmExternRef, true).exportAs("g_extern");

  builder.addFunction("init", kSig_v_r)
      .addLocals(kWasmAnyRef, 1)
      .addBody([
        ...wasmI32Const(42),
        kGCPrefix, kExprStructNew, elem_type,
        kExprLocalTee, 1,
        kExprLocalGet, 0,
        kGCPrefix, kExprStructNew, struct_type,
        kExprGlobalSet, g_struct.index,

        kExprLocalGet, 1,
        kExprI32Const, 4,
        kGCPrefix, kExprArrayNew, array_type,
        kExprGlobalSet, g_array.index,

        kExprLocalGet, 0,
        kExprGlobalSet, g_extern.index,

        kExprI32Const, 0,
        kExprLocalGet, 1,
        kExprTableSet, any_table.index,

        kExprI32Const, 0,
        kExprLocalGet, 0,
        kExprTableSet, extern_table.index,
      ]).exportFunc();

  builder.addFunction("store_nulls", kSig_v_v)
      .addBody([
        kExprGlobalGet, g_struct.index,
        kExprRefNull, kAnyRefCode,
        kGCPrefix, kExprStructSet, struct_type, 0,

        kExprGlobalGet, g_struct.index,
        kExprRefNull, kExternRefCode,
        kGCPrefix, kExprStructSet, struct_type, 1,

        kExprGlobalGet, g_array.index,
        kExprI32Const, 0,
        kExprRefNull, kAnyRefCode,
        kGCPrefix, kExprArraySet, array_type,

        kExprRefNull, kExternRefCode,
        kExprGlobalSet, g_extern.index,

        kExprI32Const, 0,
        kExprRefNull, kAnyRefCode,
        kExprTableSet, any_table.index,

        kExprI32Const, 0,
        kExprRefNull, kExternRefCode,
        kExprTableSet, extern_table.index,
      ]).exportFunc();

  builder.addFunction("get_struct_any", kSig_r_v)
      .addBody([
        kExprGlobalGet, g_struct.index,
        kGCPrefix, kExprStructGet, struct_type, 0,
        kGCPrefix, kExprExternConvertAny,
      ]).exportFunc();

  builder.addFunction("get_struct_extern", kSig_r_v)
      .addBody([
        kExprGlobalGet, g_struct.index,
        kGCPrefix, kExprStructGet, struct_type, 1,
      ]).exportFunc();

  builder.addFunction("get_array_elem", kSig_r_v)
      .addBody([
        kExprGlobalGet, g_array.index,
        kExprI32Const, 0,
        kGCPrefix, kExprArrayGet, array_type,
        kGCPrefix, kExprExternConvertAny,
      ]).exportFunc();

  const instance = builder.instantiate();
  const obj = {x: 1};
  instance.exports.init(obj);

  // Promote containers into old space so skipped write barriers are checked by
  // V8_VERIFY_WRITE_BARRIERS (in debug builds) and heap verification across GC.
  gc();
  gc();

  %WasmTierUpFunction(instance.exports.store_nulls);
  instance.exports.store_nulls();
  assertSame(null, instance.exports.get_struct_any());
  assertSame(null, instance.exports.get_struct_extern());
  assertSame(null, instance.exports.get_array_elem());
  assertSame(null, instance.exports.g_extern.value);
  assertSame(null, instance.exports.any_table.get(0));
  assertSame(null, instance.exports.extern_table.get(0));

  gc({type: "minor"});
  gc();
  assertSame(null, instance.exports.get_struct_any());
  assertSame(null, instance.exports.get_struct_extern());
  assertSame(null, instance.exports.get_array_elem());
  assertSame(null, instance.exports.g_extern.value);
  assertSame(null, instance.exports.any_table.get(0));
  assertSame(null, instance.exports.extern_table.get(0));
})();
