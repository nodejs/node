// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-extended-const

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function ExtendedConstantsTestI32() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let imported_global_0 = builder.addImportedGlobal("m", "g0", kWasmI32, false);
  let imported_global_1 = builder.addImportedGlobal("m", "g1", kWasmI32, false);

  let defined_global = builder.addGlobal(
    kWasmI32, false,
    WasmInitExpr.I32Add(
      WasmInitExpr.GlobalGet(imported_global_0),
      WasmInitExpr.I32Mul(
        WasmInitExpr.GlobalGet(imported_global_1),
        WasmInitExpr.I32Sub(
          WasmInitExpr.GlobalGet(imported_global_0),
          WasmInitExpr.I32Const(1)))));

  builder.addExportOfKind("global", kExternalGlobal, defined_global.index);

  let value0 = 123;
  let value1 = -450;

  let global_obj0 = new WebAssembly.Global({value: "i32", mutable: false},
                                           value0);
  let global_obj1 = new WebAssembly.Global({value: "i32", mutable: false},
                                           value1);

  let instance = builder.instantiate({m : {g0: global_obj0, g1: global_obj1}});

  assertEquals(value0 + (value1 * (value0 - 1)), instance.exports.global.value);
})();

(function ExtendedConstantsTestI64() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let imported_global_0 = builder.addImportedGlobal("m", "g0", kWasmI64, false);
  let imported_global_1 = builder.addImportedGlobal("m", "g1", kWasmI64, false);

  let defined_global = builder.addGlobal(
    kWasmI64, false,
    WasmInitExpr.I64Add(
      WasmInitExpr.GlobalGet(imported_global_0),
      WasmInitExpr.I64Mul(
        WasmInitExpr.GlobalGet(imported_global_1),
        WasmInitExpr.I64Sub(
          WasmInitExpr.GlobalGet(imported_global_0),
          WasmInitExpr.I64Const(1)))));

  builder.addExportOfKind("global", kExternalGlobal, defined_global.index);

  let value0 = 123n;
  let value1 = -450n;

  let global_obj0 = new WebAssembly.Global({value: "i64", mutable: false},
                                           value0);
  let global_obj1 = new WebAssembly.Global({value: "i64", mutable: false},
                                           value1);

  let instance = builder.instantiate({m : {g0: global_obj0, g1: global_obj1}});

  assertEquals(value0 + (value1 * (value0 - 1n)),
               instance.exports.global.value);
})();
