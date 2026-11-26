// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function testImportedTypedTableUsesLocalType() {
  const builderA = new WasmModuleBuilder();
  for (let i = 0; i < 6; ++i) builderA.addType(kSig_v_v);
  const tableA = builderA.addTable(wasmRefNullType(5), 1, 1);
  builderA.addExportOfKind("t", kExternalTable, tableA.index);
  const instanceA = builderA.instantiate();

  const builderB = new WasmModuleBuilder();
  const sigB = builderB.addType(kSig_v_v);
  builderB.addImportedTable("A", "t", 1, 1, wasmRefNullType(sigB));
  const f = builderB.addFunction("f", kSig_v_v).addBody([]);
  builderB.addExport("f", f.index);
  const segment = builderB.addPassiveElementSegment(
      [[kExprRefFunc, f.index]], wasmRefNullType(sigB));
  builderB.addFunction("table_init", kSig_v_v)
      .addBody([
        ...wasmI32Const(0),
        ...wasmI32Const(0),
        ...wasmI32Const(0),
        kNumericPrefix, kExprTableInit, segment, 0,
      ])
      .exportFunc();
  builderB.addFunction("table_copy", kSig_v_v)
      .addBody([
        ...wasmI32Const(0),
        ...wasmI32Const(0),
        ...wasmI32Const(0),
        kNumericPrefix, kExprTableCopy, 0, 0,
      ])
      .exportFunc();

  const instanceB = builderB.instantiate({A: {t: instanceA.exports.t}});
  instanceB.exports.table_init();
  instanceB.exports.table_copy();
})();
