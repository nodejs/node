// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

load("test/mjsunit/wasm/wasm-module-builder.js");

(function Test1() {
  var exporting_instance = (function () {
    var builder = new WasmModuleBuilder();

    var sig_index = builder.addType(kSig_i_ii);
    var wrong_sig_index = builder.addType(kSig_i_i);

    var addition_index = builder.addFunction("addition", sig_index)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add])
      .exportFunc();

    var global = builder.addGlobal(wasmRefType(sig_index), false);
    global.function_index = addition_index;
    global.exportAs("global");
    builder.addGlobal(wasmOptRefType(wrong_sig_index), false)
      .exportAs("mistyped_global");

    return builder.instantiate({});
  })();

  // Mistyped imported global.
  assertThrows(
    () => {
      var builder = new WasmModuleBuilder();
      var sig_index = builder.addType(kSig_i_ii);
      builder.addImportedGlobal("imports", "global", wasmOptRefType(sig_index),
                                false);
      builder.instantiate(
        {imports: { global: exporting_instance.exports.mistyped_global }})},
    WebAssembly.LinkError,
    /imported global does not match the expected type/
  );

  // Mistyped imported global due to cross-module typechecking.
  assertThrows(
    () => {
      var builder = new WasmModuleBuilder();
      var sig_index = builder.addType(kSig_i_i);
      builder.addImportedGlobal("imports", "global", wasmOptRefType(sig_index),
                                false);
      builder.instantiate(
        {imports: { global: exporting_instance.exports.global }})},
    WebAssembly.LinkError,
    /imported global does not match the expected type/
  );

  // Non-function imported into function-typed global.
  assertThrows(
    () => {
      var builder = new WasmModuleBuilder();
      var sig_index = builder.addType(kSig_i_ii);
      builder.addImportedGlobal("imports", "global", wasmOptRefType(sig_index),
                                false);
      builder.instantiate({imports: { global: 42 }})},
    WebAssembly.LinkError,
    /function-typed object must be null \(if nullable\) or a Wasm function object/
  );

  // Mistyped function import.
  assertThrows(
    () => {
      var builder = new WasmModuleBuilder();
      var sig_index = builder.addType(kSig_i_i);
      builder.addImportedGlobal("imports", "global", wasmRefType(sig_index),
                                false);
      builder.instantiate(
        {imports: { global: exporting_instance.exports.addition }})},
    WebAssembly.LinkError,
    /assigned exported function has to be a subtype of the expected type/
  );

  var instance = (function () {
    var builder = new WasmModuleBuilder();

    var sig_index = builder.addType(kSig_i_ii);

    builder.addImportedGlobal("imports", "global", wasmOptRefType(sig_index),
                              false);

    builder.addFunction("test_import", kSig_i_ii)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprGlobalGet, 0,
                kExprCallRef])
      .exportFunc();

    return builder.instantiate({imports: {
      global: exporting_instance.exports.global
    }});
  })();

  // This module is valid.
  assertFalse(instance === undefined);
  assertFalse(instance === null);
  assertFalse(instance === 0);

  // The correct function reference has been passed.
  assertEquals(66, instance.exports.test_import(42, 24));
})();
