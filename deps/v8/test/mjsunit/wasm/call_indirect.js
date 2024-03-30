// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestCallIndirectJSFunction() {
  print(arguments.callee.name);
  let js_function = function(a, b, c) { return c ? a : b; };

  let test = function(is_tail_call) {
    const builder = new WasmModuleBuilder();

    builder.addType(kSig_i_i);
    let sig = builder.addType(kSig_i_iii);

    let callee = builder.addImport("m", "f", kSig_i_iii);

    let table = builder.addTable(kWasmFuncRef, 10, 10);

    builder.addActiveElementSegment(table, wasmI32Const(0), [callee]);

    let left = -2;
    let right = 3;

    builder.addFunction("main", kSig_i_i)
      .addBody([...wasmI32Const(left), ...wasmI32Const(right), kExprLocalGet, 0,
                ...wasmI32Const(0),
                is_tail_call ? kExprReturnCallIndirect : kExprCallIndirect,
                sig, table.index])
      .exportFunc();

    let instance = builder.instantiate({m: {f: js_function}});

    assertEquals(left, instance.exports.main(1));
    assertEquals(right, instance.exports.main(0));
  }

  test(true);
  test(false);
})();

(function TestCallIndirectIsorecursiveTypeCanonicalization() {
  print(arguments.callee.name);

  let exporting_instance = (function() {
    const builder = new WasmModuleBuilder();

    let struct_mistyped = builder.addStruct([]);

    let struct = builder.addStruct([makeField(kWasmI32, true)]);

    let sig_mistyped = builder.addType(
        makeSig([wasmRefNullType(struct_mistyped)], [kWasmI32]));
    let sig = builder.addType(makeSig([wasmRefNullType(struct)], [kWasmI32]));

    builder.addFunction("export", sig)
      .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0,
                ...wasmI32Const(0), kExprI32GeS])
      .exportFunc();

    builder.addFunction("export_mistyped", sig_mistyped)
      .addBody([...wasmI32Const(0)])
      .exportFunc();

    return builder.instantiate();
  })();

  const builder = new WasmModuleBuilder();

  // Have these in the reverse order than before.
  let struct = builder.addStruct([makeField(kWasmI32, true)]);
  let struct_mistyped = builder.addStruct([]);

  let sig = builder.addType(makeSig([wasmRefNullType(struct)], [kWasmI32]));
  let sig_identical = builder.addType(
      makeSig([wasmRefNullType(struct)], [kWasmI32]));
  let sig_mistyped = builder.addType(
      makeSig([wasmRefNullType(struct_mistyped)], [kWasmI32]));

  let imported = builder.addImport("m", "imp", sig);
  let imported_mistyped = builder.addImport("m", "imp_m", sig_mistyped);

  let local1 = builder.addFunction("local1", sig)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0,
              ...wasmI32Const(0), kExprI32LtS])
    .index;

  let local2 = builder.addFunction("local2", sig_identical)
      .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0,
                ...wasmI32Const(0), kExprI32Eq])
      .index;

  let mistyped = builder.addFunction("mistyped", kSig_i_i)
    .addBody([kExprLocalGet, 0])
    .index;

  let table_size = 10;
  let table = builder.addTable(kWasmFuncRef, table_size, table_size);

  builder.addActiveElementSegment(
    table.index, wasmI32Const(0),
    [[kExprRefFunc, imported], [kExprRefFunc, imported_mistyped],
     [kExprRefFunc, local1], [kExprRefFunc, local2], [kExprRefFunc, mistyped]],
    table.type);

  // Params: Struct field, table index
  builder.addFunction("main", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct,
              kExprLocalGet, 1, kExprCallIndirect, sig, table.index])
    .exportFunc();

  let importing_instance =
    builder.instantiate(
      {m: {imp: exporting_instance.exports.export,
           imp_m: exporting_instance.exports.export_mistyped}});

  assertEquals(1, importing_instance.exports.main(10, imported))
  assertEquals(0, importing_instance.exports.main(-5, imported))
  assertEquals(0, importing_instance.exports.main(10, local1))
  assertEquals(1, importing_instance.exports.main(-5, local1))
  assertEquals(0, importing_instance.exports.main(10, local2))
  assertEquals(1, importing_instance.exports.main(0, local2))
  // Mistyped entries
  assertTraps(kTrapFuncSigMismatch,
              () => importing_instance.exports.main(10, mistyped));
  assertTraps(kTrapFuncSigMismatch,
              () => importing_instance.exports.main(10, imported_mistyped));
  // Null entry
  assertTraps(kTrapFuncSigMismatch,
              () => importing_instance.exports.main(10, table_size - 1));
  assertTraps(kTrapTableOutOfBounds,
              () => importing_instance.exports.main(10, table_size));
})();
