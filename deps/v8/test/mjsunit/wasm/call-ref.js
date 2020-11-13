// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-type-reflection --experimental-wasm-gc

load("test/mjsunit/wasm/wasm-module-builder.js");

(function Test1() {
  var exporting_instance = (function () {
    var builder = new WasmModuleBuilder();

    builder.addFunction("addition", kSig_i_ii)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add])
      .exportFunc();

    return builder.instantiate({});
  })();

  var instance = (function () {
    var builder = new WasmModuleBuilder();

    var sig_index = builder.addType(kSig_i_ii);

    var imported_type_reflection_function_index =
      builder.addImport("imports", "mul", sig_index);

    var imported_js_function_index =
      builder.addImport("imports", "js_add", sig_index);

    var imported_wasm_function_index =
      builder.addImport("imports", "wasm_add", sig_index);

    builder.addExport("unused", imported_wasm_function_index);
    builder.addExport("reexported_js_function", imported_js_function_index);
    builder.addExport("reexported_webassembly_function",
                      imported_type_reflection_function_index);

    var locally_defined_function =
      builder.addFunction("sub", sig_index)
        .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Sub])
        .exportFunc();

    builder.addFunction("main", makeSig(
      [wasmRefType(sig_index), kWasmI32, kWasmI32], [kWasmI32]))
      .addBody([kExprLocalGet, 1, kExprLocalGet, 2, kExprLocalGet, 0,
                kExprCallRef])
      .exportFunc();

    builder.addFunction("test_local", kSig_i_v)
      .addBody([kExprI32Const, 55, kExprI32Const, 42,
                kExprRefFunc, locally_defined_function.index, kExprCallRef])
      .exportFunc();

    builder.addFunction("test_js_import", kSig_i_v)
      .addBody([kExprI32Const, 15, kExprI32Const, 42,
                kExprRefFunc, imported_js_function_index, kExprCallRef])
      .exportFunc();

      builder.addFunction("test_wasm_import", kSig_i_v)
        .addBody([kExprI32Const, 15, kExprI32Const, 42,
                  kExprRefFunc, imported_wasm_function_index, kExprCallRef])
        .exportFunc();

    /* Future use
    builder.addFunction("test_webassembly_import", kSig_i_v)
      .addBody([kExprI32Const, 3, kExprI32Const, 7,
                kExprRefFunc, imported_type_reflection_function_index,
                kExprCallRef])
      .exportFunc();
    */

    return builder.instantiate({imports: {
      js_add: function(a, b) { return a + b; },
      wasm_add: exporting_instance.exports.addition,
      mul: new WebAssembly.Function({parameters:['i32', 'i32'],
                                     results: ['i32']},
                                    function(a, b) { return a * b; })
    }});
  })();

  // Check the modules exist.
  assertFalse(instance === undefined);
  assertFalse(instance === null);
  assertFalse(instance === 0);
  assertEquals("object", typeof instance.exports);
  assertEquals("function", typeof instance.exports.main);

  print("--locally defined func--");
  assertEquals(13, instance.exports.test_local());
  print("--locally defined exported func--")
  assertEquals(5, instance.exports.main(instance.exports.sub, 12, 7));

  print("--imported js func--");
  assertEquals(57, instance.exports.test_js_import());
  print("--imported and reexported js func--")
  assertEquals(19, instance.exports.main(
    instance.exports.reexported_js_function, 12, 7));

  print("--imported function from another module--");
  assertEquals(57, instance.exports.test_wasm_import());
  print("--not imported function defined in another module--");
  assertEquals(19, instance.exports.main(
    exporting_instance.exports.addition, 12, 7));

  // TODO(7748): Make these work once we know how we interact
  //             with the 'type reflection' proposal.
  //print("--imported WebAssembly.Function--")
  //assertEquals(21, instance.exports.test_webassembly_import());
  //print(" --not imported WebAssembly.Function--")
})();
