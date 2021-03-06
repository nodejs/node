// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-typed-funcref

load("test/mjsunit/wasm/wasm-module-builder.js");

(function Test1() {
  var exporting_instance = (function () {
    var builder = new WasmModuleBuilder();
    var binary_type = builder.addType(kSig_i_ii);

    builder.addFunction("addition", kSig_i_ii)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add])
      .exportFunc();

    builder.addFunction("id", kSig_i_i)
      .addBody([kExprLocalGet, 0])
      .exportFunc();

    builder.addTable(wasmOptRefType(binary_type), 1, 100).exportAs("table");

    return builder.instantiate({});
  })();

  // Wrong type for imported table.
  assertThrows(
    () => {
      var builder = new WasmModuleBuilder();
      var unary_type = builder.addType(kSig_i_i);
      builder.addImportedTable("imports", "table", 1, 100,
                               wasmOptRefType(unary_type));
      builder.instantiate({imports: {table: exporting_instance.exports.table}})
    },
    WebAssembly.LinkError,
    /imported table does not match the expected type/
  )

  // Type for imported table must match exactly.
  assertThrows(
    () => {
      var builder = new WasmModuleBuilder();
      builder.addImportedTable("imports", "table", 1, 100, kWasmFuncRef);
      builder.instantiate({imports: {table: exporting_instance.exports.table}})
    },
    WebAssembly.LinkError,
    /imported table does not match the expected type/
  )

  var instance = (function () {
    var builder = new WasmModuleBuilder();

    var unary_type = builder.addType(kSig_i_i);
    var binary_type = builder.addType(kSig_i_ii);

    builder.addImportedTable("imports", "table", 1, 100,
                             wasmOptRefType(binary_type));

    var table = builder.addTable(wasmOptRefType(unary_type), 1)
                  .exportAs("table");
    builder.addTable(kWasmFuncRef, 1).exportAs("generic_table");

    builder.addFunction("table_test", makeSig([wasmRefType(unary_type)],
                                              [kWasmI32]))
      // Set table[0] to input function, then retrieve it and call it.
      .addBody([kExprI32Const, 0, kExprLocalGet, 0, kExprTableSet, table.index,
                kExprI32Const, 42, kExprI32Const, 0, kExprTableGet, table.index,
                kExprCallRef])
      .exportFunc();

    // Instantiate with a table of the correct type.
    return builder.instantiate(
      {imports: {table: exporting_instance.exports.table}});
  })();

  // This module is valid.
  assertTrue(!!instance);

  // The correct function reference is preserved when setting it to and getting
  // it back from a table.
  assertEquals(42, instance.exports.table_test(exporting_instance.exports.id));

  // Setting from JS API respects types.
  instance.exports.generic_table.set(0, exporting_instance.exports.id);
  instance.exports.table.set(0, exporting_instance.exports.id);
  assertThrows(
    () => instance.exports.table.set(0, exporting_instance.exports.addition),
    TypeError,
    /Argument 1 must be null or a WebAssembly function of type compatible to 'this'/);
})();
