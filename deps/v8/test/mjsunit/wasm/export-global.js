// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/wasm/wasm-module-builder.js");

(function duplicateGlobalExportName() {
  var builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI64, false).exportAs('g');
  builder.addGlobal(kWasmI64, false).exportAs('g');
  assertThrows(
      () => builder.instantiate(), WebAssembly.CompileError,
      /Duplicate export name 'g' for global 0 and global 1/);
})();

(function exportNameClashWithFunction() {
  var builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI64, false).exportAs('foo');
  builder.addFunction('f', kSig_v_v).addBody([]).exportAs('foo');
  assertThrows(
      () => builder.instantiate(), WebAssembly.CompileError,
      /Duplicate export name 'foo' for global 0 and function 0/);
})();

(function veryLongExportName() {
  // Regression test for crbug.com/740023.
  var export_name = 'abc';
  while (export_name.length < 8192) {
    export_name = export_name.concat(export_name);
  }
  var builder = new WasmModuleBuilder();
  var global = builder.addGlobal(kWasmI64, false);
  global.exportAs(export_name);
  global.exportAs(export_name);
  var error_msg =
      'Duplicate export name \'(abc){10,20}ab?c?\.\.\.\' for global 0 and global 0';
  assertThrows(
      () => builder.instantiate(), WebAssembly.CompileError,
      new RegExp(error_msg));
})();
