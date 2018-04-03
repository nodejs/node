// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --allow-natives-syntax

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

var expect_elison = 0;
var expect_no_elison = 1;
// function calls stack: first_export -> first_func -> first_import ->
// second_export -> second_import
// In this case, first_import and second_export have same signature,
// So that wrappers will be removed
(function TestWasmWrapperElision() {
    var imported = function (a) {
        return a;
    };

    var second_module = new WasmModuleBuilder();
    var sig_index = second_module.addType(kSig_i_i);
    second_module
        .addImport("import_module_2", "import_name_2", sig_index);
    second_module
        .addFunction("second_export", sig_index)
        .addBody([
            kExprGetLocal, 0,
            kExprCallFunction, 0,
            kExprReturn
        ])
        .exportFunc();

    var first_module = new WasmModuleBuilder();
    var sig_index = first_module.addType(kSig_i_i);
    first_module
        .addImport("import_module_1", "import_name_1", sig_index);
    first_module
        .addFunction("first_export", sig_index)
        .addBody([
            kExprGetLocal, 0,
            kExprCallFunction, 2,
            kExprReturn
        ])
        .exportFunc();
    first_module
        .addFunction("first_func", sig_index)
        .addBody([
            kExprI32Const, 1,
            kExprGetLocal, 0,
            kExprI32Add,
            kExprCallFunction, 0,
            kExprReturn
        ]);

    var f = second_module
        .instantiate({import_module_2: {import_name_2: imported}})
        .exports.second_export;
    var the_export = first_module
        .instantiate({import_module_1: {import_name_1: f}})
        .exports.first_export;
    assertEquals(the_export(2), 3);
    assertEquals(the_export(-1), 0);
    assertEquals(the_export(0), 1);
    assertEquals(the_export(5.5), 6);
    assertEquals(%CheckWasmWrapperElision(the_export, expect_elison), true);
})();

// Function calls stack: first_export -> first_func -> first_import ->
// second_export -> second_import
// In this test, first_import and second_export have the same signature, and
// therefore the wrappers will be removed. If the wrappers are not removed, then
// the test crashes because of the int64 parameter, which is not allowed in the
// wrappers.
(function TestWasmWrapperElisionInt64() {
 var imported = function (a) {
     return a;
 };

 var second_module = new WasmModuleBuilder();
 var sig_index1 = second_module.addType(kSig_i_i);
 var sig_index_ll = second_module.addType(kSig_l_l);
 second_module
     .addImport("import_module_2", "import_name_2", sig_index1);
 second_module
     .addFunction("second_export", sig_index_ll)
     .addBody([
         kExprGetLocal, 0,
         kExprI32ConvertI64,
         kExprCallFunction, 0,
         kExprI64SConvertI32,
         kExprReturn
     ])
     .exportFunc();

 var first_module = new WasmModuleBuilder();
 var sig_index = first_module.addType(kSig_i_v);
 var sig_index_ll = first_module.addType(kSig_l_l);
 first_module
     .addImport("import_module_1", "import_name_1", sig_index_ll);
 first_module
     .addFunction("first_export", sig_index)
     .addBody([
         kExprI64Const, 2,
         kExprCallFunction, 2,
         kExprI32ConvertI64,
         kExprReturn
     ])
     .exportFunc();
 first_module
     .addFunction("first_func", sig_index_ll)
     .addBody([
         kExprI64Const, 1,
         kExprGetLocal, 0,
         kExprI64Add,
         kExprCallFunction, 0,
         kExprReturn
     ]);

 var f = second_module
     .instantiate({import_module_2: {import_name_2: imported}})
     .exports.second_export;
 var the_export = first_module
     .instantiate({import_module_1: {import_name_1: f}})
     .exports.first_export;
   assertEquals(the_export(), 3);
})();

// function calls stack: first_export -> first_func -> first_import ->
// second_export -> second_import
// In this case, second_export has fewer params than first_import,
// so instantiation should fail.
assertThrows(function TestWasmWrapperNoElisionLessParams() {
    var imported = function (a) {
        return a;
    };

    var second_module = new WasmModuleBuilder();
    var sig_index_1 = second_module.addType(kSig_i_i);
    second_module
        .addImport("import_module_2", "import_name_2", sig_index_1);
    second_module
        .addFunction("second_export", sig_index_1)
        .addBody([
            kExprGetLocal, 0,
            kExprCallFunction, 0,
            kExprReturn
        ])
        .exportFunc();

    var first_module = new WasmModuleBuilder();
    var sig_index_2 = first_module.addType(kSig_i_ii);
    first_module
        .addImport("import_module_1", "import_name_1", sig_index_2);
    first_module
        .addFunction("first_export", sig_index_2)
        .addBody([
            kExprGetLocal, 0,
            kExprGetLocal, 1,
            kExprCallFunction, 2,
            kExprReturn
        ])
        .exportFunc();
    first_module
        .addFunction("first_func", sig_index_2)
        .addBody([
            kExprGetLocal, 0,
            kExprGetLocal, 1,
            kExprCallFunction, 0,
            kExprReturn
        ]);

    var f = second_module
        .instantiate({import_module_2: {import_name_2: imported}})
        .exports.second_export;
    var the_export = first_module
        .instantiate({import_module_1: {import_name_1: f}})
        .exports.first_export;
    assertEquals(the_export(4, 5), 4);
    assertEquals(the_export(-1, 4), -1);
    assertEquals(the_export(0, 2), 0);
    assertEquals(the_export(9.9, 4.3), 9);
    assertEquals(%CheckWasmWrapperElision(the_export, expect_no_elison), true);
});

// function calls stack: first_export -> first_func -> first_import ->
// second_export -> second_import
// In this case, second_export has more params than first_import,
// so instantiation should fail.
assertThrows(function TestWasmWrapperNoElisionMoreParams() {
    var imported = function (a, b, c) {
        return a+b+c;
    };

    var second_module = new WasmModuleBuilder();
    var sig_index_3 = second_module.addType(kSig_i_iii);
    second_module
        .addImport("import_module_2", "import_name_2", sig_index_3);
    second_module
        .addFunction("second_export", sig_index_3)
        .addBody([
            kExprGetLocal, 0,
            kExprGetLocal, 1,
            kExprGetLocal, 2,
            kExprCallFunction, 0,
            kExprReturn
        ])
        .exportFunc();

    var first_module = new WasmModuleBuilder();
    var sig_index_2 = first_module.addType(kSig_i_ii);
    first_module
        .addImport("import_module_1", "import_name_1", sig_index_2);
    first_module
        .addFunction("first_export", sig_index_2)
        .addBody([
            kExprGetLocal, 0,
            kExprGetLocal, 1,
            kExprCallFunction, 2,
            kExprReturn
        ])
        .exportFunc();
    first_module
        .addFunction("first_func", sig_index_2)
        .addBody([
            kExprGetLocal, 0,
            kExprGetLocal, 1,
            kExprCallFunction, 0,
            kExprReturn
        ]);

    var f = second_module
        .instantiate({import_module_2: {import_name_2: imported}})
        .exports.second_export;
    var the_export = first_module
        .instantiate({import_module_1: {import_name_1: f}})
        .exports.first_export;
    assertEquals(the_export(5, 6), 11);
    assertEquals(the_export(-1, -4), -5);
    assertEquals(the_export(0, 0), 0);
    assertEquals(the_export(1.1, 2.7), 3);
    assertEquals(%CheckWasmWrapperElision(the_export, expect_no_elison), true);
});

// function calls stack: first_export -> first_func -> first_import ->
// second_export -> second_import
// In this case, second_export has different params type with first_import,
// so instantiation should fail.
assertThrows(function TestWasmWrapperNoElisionTypeMismatch() {
    var imported = function (a, b) {
        return a+b;
    };

    var second_module = new WasmModuleBuilder();
    var sig_index_2 = second_module.addType(kSig_d_dd);
    second_module
        .addImport("import_module_2", "import_name_2", sig_index_2);
    second_module
        .addFunction("second_export", sig_index_2)
        .addBody([
            kExprGetLocal, 0,
            kExprGetLocal, 1,
            kExprCallFunction, 0,
            kExprReturn
        ])
        .exportFunc();

    var first_module = new WasmModuleBuilder();
    var sig_index_2 = first_module.addType(kSig_i_ii);
    first_module
        .addImport("import_module_1", "import_name_1", sig_index_2);
    first_module
        .addFunction("first_export", sig_index_2)
        .addBody([
            kExprGetLocal, 0,
            kExprGetLocal, 1,
            kExprCallFunction, 2,
            kExprReturn
        ])
        .exportFunc();
    first_module
        .addFunction("first_func", sig_index_2)
        .addBody([
            kExprGetLocal, 0,
            kExprGetLocal, 1,
            kExprCallFunction, 0,
            kExprReturn
        ]);

    var f = second_module
        .instantiate({import_module_2: {import_name_2: imported}})
        .exports.second_export;
    var the_export = first_module
        .instantiate({import_module_1: {import_name_1: f}})
        .exports.first_export;
    assertEquals(the_export(2.8, 9.1), 11);
    assertEquals(the_export(-1.7, -2.5), -3);
    assertEquals(the_export(0.0, 0.0), 0);
    assertEquals(the_export(2, -2), 0);
    assertEquals(%CheckWasmWrapperElision(the_export, expect_no_elison), true);
});


(function TestSimpleI64Ret() {
  var builder = new WasmModuleBuilder();
  builder.addFunction("exp", kSig_l_v)
    .addBody([
      kExprI64Const, 23
    ])
    .exportFunc();
  var exported = builder.instantiate().exports.exp;

  var builder = new WasmModuleBuilder();
  builder.addImport("imp", "func", kSig_l_v);
  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprCallFunction, 0,
      kExprI32ConvertI64
    ])
    .exportFunc();

  var instance = builder.instantiate({imp: {func: exported}});

  assertEquals(23, instance.exports.main());

})();
