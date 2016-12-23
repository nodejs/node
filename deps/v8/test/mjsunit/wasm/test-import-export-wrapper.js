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
        .addImportWithModule("import_module_2", "import_name_2", sig_index);
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
        .addImportWithModule("import_module_1", "import_name_1", sig_index);
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

// function calls stack: first_export -> first_func -> first_import ->
// second_export -> second_import
// In this case, second_export has less params than first_import,
// So that wrappers will not be removed
(function TestWasmWrapperNoElisionLessParams() {
    var imported = function (a) {
        return a;
    };

    var second_module = new WasmModuleBuilder();
    var sig_index_1 = second_module.addType(kSig_i_i);
    second_module
        .addImportWithModule("import_module_2", "import_name_2", sig_index_1);
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
        .addImportWithModule("import_module_1", "import_name_1", sig_index_2);
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
})();

// function calls stack: first_export -> first_func -> first_import ->
// second_export -> second_import
// In this case, second_export has more params than first_import,
// So that wrappers will not be removed
(function TestWasmWrapperNoElisionMoreParams() {
    var imported = function (a, b, c) {
        return a+b+c;
    };

    var second_module = new WasmModuleBuilder();
    var sig_index_3 = second_module.addType(kSig_i_iii);
    second_module
        .addImportWithModule("import_module_2", "import_name_2", sig_index_3);
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
        .addImportWithModule("import_module_1", "import_name_1", sig_index_2);
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
})();

// function calls stack: first_export -> first_func -> first_import ->
// second_export -> second_import
// In this case, second_export has different params type with first_import,
// So that wrappers will not be removed
(function TestWasmWrapperNoElisionTypeMismatch() {
    var imported = function (a, b) {
        return a+b;
    };

    var second_module = new WasmModuleBuilder();
    var sig_index_2 = second_module.addType(kSig_d_dd);
    second_module
        .addImportWithModule("import_module_2", "import_name_2", sig_index_2);
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
        .addImportWithModule("import_module_1", "import_name_1", sig_index_2);
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
})();
