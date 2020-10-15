// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-type-reflection --experimental-wasm-gc

load("test/mjsunit/wasm/wasm-module-builder.js");

(function Test1() {
  var instance = (function () {
    var builder = new WasmModuleBuilder();

    var sig_index = builder.addType(kSig_i_ii);

    var imported_webassembly_function_index =
      builder.addImport("imports", "mul", sig_index);

    var imported_js_function_index =
      builder.addImport("imports", "add", sig_index);

    builder.addExport("reexported_js_function",
                      imported_js_function_index);
    builder.addExport("reexported_webassembly_function",
                      imported_webassembly_function_index);

    var locally_defined_function =
      builder.addFunction("sub", sig_index)
        .addBody([
          kExprLocalGet, 0,
          kExprLocalGet, 1,
          kExprI32Sub
        ])
        .exportFunc();

    builder.addFunction("main", makeSig([kWasmAnyFunc, kWasmI32, kWasmI32],
                                        [kWasmI32]))
      .addBody([
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kExprLocalGet, 0,
        kGCPrefix, kExprRttCanon, 0,
        kGCPrefix, kExprRefCast, kWasmAnyFunc, 0,
        kExprCallRef
      ])
      .exportFunc();

    builder.addFunction("test_local", makeSig([], [kWasmI32]))
    .addBody([
      kExprI32Const, 55,
      kExprI32Const, 42,
      kExprRefFunc, locally_defined_function.index,
      kExprCallRef
    ])
    .exportFunc();

    builder.addFunction("test_js_import", makeSig([], [kWasmI32]))
      .addBody([
        kExprI32Const, 15,
        kExprI32Const, 42,
        kExprRefFunc, imported_js_function_index,
        kExprCallRef
      ])
      .exportFunc();

    builder.addFunction("test_webassembly_import", makeSig([], [kWasmI32]))
      .addBody([
        kExprI32Const, 3,
        kExprI32Const, 7,
        kExprRefFunc, imported_webassembly_function_index,
        kExprCallRef
      ])
      .exportFunc();

    return builder.instantiate({imports: {
      add: function(a, b) { return a + b; },
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

  // TODO(7748): Make this work.
  //print("--imported WebAssembly.Function--")
  //assertEquals(21, instance.exports.test_webassembly_import());

  //print(" --not imported WebAssembly.Function--")
})();
