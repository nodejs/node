// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-anyref --experimental-wasm-bulk-memory

load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestAnyRefTableSetWithMultipleTypes() {
  print(arguments.callee.name);
  let table = new WebAssembly.Table({element: "anyref", initial: 10});

  // Table should be initialized with null.
  assertEquals(null, table.get(1));
  let obj = {'hello' : 'world'};
  table.set(2, obj);
  assertSame(obj, table.get(2));
  table.set(3, 1234);
  assertEquals(1234, table.get(3));
  table.set(4, 123.5);
  assertEquals(123.5, table.get(4));
  table.set(5, undefined);
  assertEquals(undefined, table.get(5));
  // Overwrite entry 4, because null would otherwise be the default value.
  table.set(4, null);
  assertEquals(null, table.get(4));
  table.set(7, print);
  assertEquals(print, table.get(7));

  assertThrows(() => table.set(12), RangeError);
})();

(function TestImportAnyRefTable() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  const table_index = builder.addImportedTable("imp", "table", 3, 10, kWasmAnyRef);
  builder.addFunction('get', kSig_r_v)
  .addBody([kExprI32Const, 0, kExprTableGet, table_index]);

  let table_ref = new WebAssembly.Table({element: "anyref", initial: 3, maximum: 10});
  builder.instantiate({imp:{table: table_ref}});

  let table_func = new WebAssembly.Table({ element: "anyfunc", initial: 3, maximum: 10 });
  assertThrows(() => builder.instantiate({ imp: { table: table_func } }),
    WebAssembly.LinkError, /imported table does not match the expected type/);
})();

(function TestAnyRefDropDeclarativeElementSegment() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  builder.addDeclarativeElementSegment([null]);
  builder.addFunction('drop', kSig_v_v)
      .addBody([kNumericPrefix, kExprElemDrop, 0])
      .exportFunc();
  const instance = builder.instantiate();

  // Counts as double-drop because declarative segments are dropped on
  // initialization and is therefore not expected to throw.
  instance.exports.drop();
})();

(function TestAnyRefTableInitFromDeclarativeElementSegment() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  const table = builder.addTable(kWasmAnyFunc, 10);
  builder.addDeclarativeElementSegment([null]);
  builder.addFunction('init', kSig_v_v)
      .addBody([
        kExprI32Const, 0, kExprI32Const, 0, kExprI32Const, 1, kNumericPrefix,
        kExprTableInit, table.index, 0
      ])
      .exportFunc();
  const instance = builder.instantiate();

  assertTraps(kTrapTableOutOfBounds, () => instance.exports.init());
})();
